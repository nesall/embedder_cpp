#include "instregistry.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <vector>
#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#include <sys/utsname.h>
#endif
#include <utils_log/logger.hpp>

using json = nlohmann::json;

namespace {
  std::mutex &registryMutex() {
    static std::mutex m;
    return m;
  }

  std::string getRegistryPath() {
    // Priority:
    // 1. Environment variable
    const char *envPath = std::getenv("EMBEDDER_REGISTRY");
    if (envPath) return envPath;

    // 2. User home directory (shared across projects)
#ifdef _WIN32
    const char *home = std::getenv("USERPROFILE");
#else
    const char *home = std::getenv("HOME");
#endif

    if (home) {
      return std::string(home) + "/.embedder_instances.json";
    }

    // 3. Fallback to current directory
    return ".embedder_instances.json";
  }
}

struct InstanceRegistry::Impl {
  std::string registryPath_;
  std::string instanceId_;
  std::thread heartbeatThread_;
  std::atomic_bool running_{ false };

  explicit Impl(const std::string &path)
    : registryPath_(path) {
    if (path.empty()) {
      registryPath_ = getRegistryPath();
    }
    instanceId_ = generateInstanceId();
  }

  ~Impl() {
    stopHeartbeat();
    unregister();
  }

  json fetchRegistry() const {
    if (!std::filesystem::exists(registryPath_)) {
      return json{ {"instances", json::array()} };
    }
    std::ifstream file(registryPath_);
    json registry;
    file >> registry;
    return registry;
  }

  void saveRegistry(const json &registry) const {
    std::ofstream file(registryPath_);
    file << registry.dump(2);
  }

  void updateHeartbeat(json &registry) const {
    for (auto &inst : registry["instances"]) {
      if (inst["id"] == instanceId_) {
        inst["last_heartbeat"] = std::time(nullptr);
        break;
      }
    }
  }

  static void cleanStaleInstances(json &registry) {
    auto &instances = registry["instances"];
    auto now = std::time(nullptr);
    instances.erase(
      std::remove_if(instances.begin(), instances.end(),
        [now](const json &inst) {
          time_t lastHeartbeat = inst["last_heartbeat"];
          if (60 < now - lastHeartbeat) return true;
          int pid = inst["pid"];
          if (!isProcessRunning(pid)) return true;
          return false;
        }),
      instances.end());
  }

  static std::string generateInstanceId() {
    char hostname[256] = { 0 };
#ifdef _WIN32
    DWORD sz = sizeof(hostname);
    GetComputerNameA(hostname, &sz);
#else
    gethostname(hostname, sizeof(hostname));
#endif
    std::stringstream ss;
    ss << hostname << "-" << getProcessId() << "-" << std::time(nullptr);
    return ss.str();
  }

  std::string detectProjectName() const {
    // TODO: questonable..
    auto path = std::filesystem::current_path();
    return path.filename().string();
  }

  static int getProcessId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
  }

  static bool isProcessRunning(int pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (h) { CloseHandle(h); return true; }
    return false;
#else
    return kill(pid, 0) == 0;
#endif
  }

  void registerInstance(int port, const std::string &name) const {
    std::lock_guard<std::mutex> lock(registryMutex());
    json registry = fetchRegistry();

    auto now = std::time(nullptr);
    std::tm tm_now = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    std::string nowStr = oss.str();

    json instance = {
        {"id", instanceId_},
        {"pid", getProcessId()},
        {"port", port},
        {"host", "localhost"},
        {"name", name.empty() ? detectProjectName() : name},
        {"started_at", now},
        {"started_at_str", nowStr},
        {"last_heartbeat", now},
        {"last_heartbeat_str", nowStr},
        {"cwd", std::filesystem::current_path().string()},
        {"status", "healthy"}
    };

    Impl::cleanStaleInstances(registry);

    bool found = false;
    for (auto &inst : registry["instances"]) {
      if (inst["id"] == instanceId_) {
        inst = instance;
        found = true;
        break;
      }
    }
    if (!found) registry["instances"].push_back(instance);

    saveRegistry(registry);
    LOG_MSG << "Registered instance:" << instanceId_ << "on port" << port;
  }

  void unregister() {
    std::lock_guard<std::mutex> lock(registryMutex());
    json registry = fetchRegistry();
    auto &instances = registry["instances"];
    instances.erase(
      std::remove_if(instances.begin(), instances.end(),
        [this](const json &inst) { return inst["id"] == instanceId_; }),
      instances.end());
    saveRegistry(registry);
    LOG_MSG << "Unregistered instance:" << instanceId_;
  }

  void startHeartbeat() {
    running_ = true;
    heartbeatThread_ = std::thread([this]() {
      while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::lock_guard<std::mutex> lock(registryMutex());
        auto registry = fetchRegistry();
        if (running_) updateHeartbeat(registry);
        Impl::cleanStaleInstances(registry);
        saveRegistry(registry);
      }
      });
  }

  void stopHeartbeat() {
    running_ = false;
    if (heartbeatThread_.joinable()) heartbeatThread_.join();
  }

  std::vector<json> getActiveInstances() {
    std::lock_guard<std::mutex> lock(registryMutex());
    json registry = fetchRegistry();
    Impl::cleanStaleInstances(registry);
    saveRegistry(registry);
    std::vector<json> active;
    auto now = std::time(nullptr);
    for (const auto &inst : registry["instances"]) {
      time_t lastHeartbeat = inst["last_heartbeat"];
      if (now - lastHeartbeat < 30) active.push_back(inst);
    }
    return active;
  }
};


InstanceRegistry::InstanceRegistry(const std::string &registryPath_)
  : imp(std::make_unique<Impl>(registryPath_))
{
}

InstanceRegistry::~InstanceRegistry() = default;

void InstanceRegistry::registerInstance(int port, const std::string &name)
{
  imp->registerInstance(port, name);
}

void InstanceRegistry::unregister()
{
  imp->unregister();
}

void InstanceRegistry::startHeartbeat()
{
  imp->startHeartbeat();
}

void InstanceRegistry::stopHeartbeat()
{
  imp->stopHeartbeat();
}

std::vector<json> InstanceRegistry::getActiveInstances() const
{
  return imp->getActiveInstances();
}

std::string InstanceRegistry::getInstanceId() const
{
  return imp->instanceId_;
}
