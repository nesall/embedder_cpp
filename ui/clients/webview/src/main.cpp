#include <webview/webview.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <utils_log/logger.hpp>
#include "wb.h"
#include "procmngr.h"
#include <filesystem>
#include <string>
#include <cassert>
#include <thread>
#include "fmt/core.h"
#include <sstream>
#include <atomic>
#include <fstream>
#include <unordered_map>
#include <random>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "WinDarkTitlebarImpl.h"
#else
#include <limits.h>
#include <unistd.h>
#endif


namespace fs = std::filesystem;

namespace {

  struct AppConfig {
    int width = 700;
    int height = 900;
    int port = 8590;
    std::string host = "127.0.0.1";
    std::unordered_map<std::string, std::string> uiPrefs;
    mutable std::mutex mutex_;

    nlohmann::json toJson() const {
      nlohmann::json j;
      j["window"] = {
          {"width", width},
          {"height", height}
      };
      j["api"] = {
          {"host", host},
          {"port", port}
      };
      j["uiPrefs"] = nlohmann::json::array();
      for (const auto &item : uiPrefs) {
        j["uiPrefs"].push_back({
          {"key", item.first},
          {"value", item.second}
        });
      }
      return j;
    }
  };

  std::string getConfigPath() {
    const std::string exeDir = Webview::getExecutableDir();
    static std::vector<std::string> paths = {
      exeDir + "/appconfig.json",
      exeDir + "/../appconfig.json",
      exeDir + "/../../appconfig.json"
    };
    std::string prefsPath;
    for (const auto &path : paths) {
      if (fs::exists(path)) {
        prefsPath = path;
        break;
      }
    }
    if (!fs::exists(prefsPath)) {
      prefsPath = paths[0];
    }
    return prefsPath;
  }

  void savePrefsToFile(const AppConfig &prefs) {
    const auto prefsPath = getConfigPath();
    nlohmann::json j = prefs.toJson();
    std::ofstream out(prefsPath);
    if (out.is_open()) {
      out << j.dump(2) << std::endl;
      out.close();
      LOG_MSG << "Updated appconfig.json with new server URL";
    } else {
      LOG_MSG << "Failed to update" << prefsPath;
      throw std::runtime_error("Failed to update appconfig.json");
    }
  }

  void fetchOrCreatePrefsJson(AppConfig &prefs) {
    LOG_START;
    const auto prefsPath = getConfigPath();
    std::ifstream f(prefsPath);
    if (f.is_open()) {
      std::stringstream ss;
      ss << f.rdbuf();
      try {
        auto j = nlohmann::json::parse(ss.str());
        if (j.contains("window") && j["window"].is_object()) {
          const auto &w = j["window"];
          if (w.contains("width") && w["width"].is_number_integer()) {
            prefs.width = w["width"].get<int>();
          }
          if (w.contains("height") && w["height"].is_number_integer()) {
            prefs.height = w["height"].get<int>();
          }
        }
        if (j.contains("api") && j["api"].is_object()) {
          const auto &w = j["api"];
          if (w.contains("host") && w["host"].is_string()) {
            prefs.host = w["host"];
          }
          if (w.contains("port") && w["port"].is_number_integer()) {
            prefs.port = w["port"].get<int>();
          }
        }
        if (j.contains("uiPrefs") && j["uiPrefs"].is_array()) {
          for (const auto &item : j["uiPrefs"]) {
            if (item.contains("key") && item.contains("value") &&
                item["key"].is_string() && item["value"].is_string()) {
              prefs.uiPrefs.insert({
                item["key"].get<std::string>(),
                item["value"].get<std::string>()
                });
            }
          }
        }
      } catch (const std::exception &e) {
        LOG_MSG << "Error parsing appconfig.json:" << e.what();
      }
    } else {
      nlohmann::json j = prefs.toJson();
      try {
        std::ofstream out(prefsPath);
        if (out.is_open()) {
          out << j.dump(2) << std::endl;
          out.close();
          LOG_MSG << "Created default appconfig.json at:" << prefsPath;
        } else {
          LOG_MSG << "Failed to create appconfig.json at:" << prefsPath;
        }
      } catch (const std::exception &e) {
        LOG_MSG << "Error writing appconfig.json:" << e.what();
      }
    }
    if (prefs.host == "localhost") prefs.host = "127.0.0.1";
    prefs.width = (std::min)((std::max)(prefs.width, 200), 1400);
    prefs.height = (std::min)((std::max)(prefs.height, 300), 1000);
  }

  std::string hashString(const std::string &str) {
    std::hash<std::string> hasher;
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hasher(str);
    return ss.str();
  }

  std::string getProjectId(const std::string &path)
  {
    nlohmann::json j;
    std::ifstream file(path);
    if (!file.is_open()) {
      throw std::runtime_error("Cannot open settings file: " + path);
    }
    file >> j;
    std::string s;
    s = j["source"].value("project_id", "");
    if (s.empty()) {
      // Auto-generate
      auto absPath = std::filesystem::absolute(path).lexically_normal();
      std::string dirName = absPath.parent_path().filename().string();
      std::string pathHash = hashString(absPath.generic_string()).substr(0, 8);
      s = dirName + "-" + pathHash;
    }
    return s;
  }

  std::string generateAppKey() {
    // Random 32-character hex string
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::stringstream ss;
    for (int i = 0; i < 16; i++) {
      ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    return ss.str();
  }

  struct ProcessesHolder {
    mutable std::mutex mutex;

    ProcessManager *getOrCreateProcess(const std::string &appKey, const std::string &projectId) {
      std::lock_guard<std::mutex> lock(mutex);
      auto it = embedderProcesses_.find(appKey);
      if (it != embedderProcesses_.end()) {
        return it->second.get();
      }
      auto procMgr = std::make_unique<ProcessManager>();
      ProcessManager *procPtr = procMgr.get();
      embedderProcesses_[appKey] = std::move(procMgr);
      projectIdToAppKey_[projectId] = appKey;
      appKeyToProjectId_[appKey] = projectId;
      return procPtr;
    }

    void discardProcess(const std::string &appKey) {
      std::lock_guard<std::mutex> lock(mutex);
      auto it = embedderProcesses_.find(appKey);
      if (it != embedderProcesses_.end()) {
        embedderProcesses_.erase(it);
        auto projIt = appKeyToProjectId_.find(appKey);
        if (projIt != appKeyToProjectId_.end()) {
          projectIdToAppKey_.erase(projIt->second);
          appKeyToProjectId_.erase(projIt);
        }
      }
    }

    ProcessManager *getProcessWithApiKey(const std::string &appKey) const {
      std::lock_guard<std::mutex> lock(mutex);
      auto it = embedderProcesses_.find(appKey);
      if (it != embedderProcesses_.end()) {
        return it->second.get();
      }
      return nullptr;
    }

    std::string getApiKeyFromProjectId(const std::string &projectId) const {
      std::lock_guard<std::mutex> lock(mutex);
      auto it = projectIdToAppKey_.find(projectId);
      if (it != projectIdToAppKey_.end()) {
        return it->second;
      }
      return "";
    }

    void waitToStopThenTerminate() {
      for (auto &proc : embedderProcesses_) {
        if (proc.second->waitForCompletion(10000)) {
          LOG_MSG << "Embedder process" << proc.second->getProcessId() << "exited cleanly";
        } else {
          LOG_MSG << "Embedder process" << proc.second->getProcessId() << "did not exit in time, terminating...";
          proc.second->stopProcess();
        }
      }
    }

  private:
    std::unordered_map<std::string, std::unique_ptr<ProcessManager>> embedderProcesses_;
    std::unordered_map<std::string, std::string> projectIdToAppKey_; // we assume 1 to 1 relationship
    std::unordered_map<std::string, std::string> appKeyToProjectId_;
  };

} // anonymous namespace

int main() {
  LOG_START;
  const std::string assetsPath = Webview::findWebAssets();
  if (assetsPath.empty()) {
    LOG_MSG << "Error: Could not find web assets (index.html)";
    LOG_MSG << "Please build the SPA client first:";
    LOG_MSG << "  cd ../spa-svelte && npm run build";
    return 1;
  }

  ProcessesHolder procUtil;

  AppConfig prefs;
  fetchOrCreatePrefsJson(prefs);

  LOG_MSG << "Loading Svelte app from: " << fs::absolute(assetsPath).string();

  httplib::Server svr;
  svr.set_mount_point("/", fs::absolute(assetsPath).string().c_str());

  svr.set_logger([](const auto &req, const auto &res) {
    LOG_MSG << req.method << req.path << "->" << res.status;
    });

  svr.Get("/api/.*", [&prefs](const httplib::Request &req, httplib::Response &res) {
    LOG_START;
    LOG_MSG << "svr.Get" << req.method << req.path;
    std::string host;
    int port;
    {
      std::lock_guard<std::mutex> lock(prefs.mutex_);
      host = prefs.host;
      port = prefs.port;
    }

    httplib::Client cli(host, port);
    cli.set_connection_timeout(0, 60 * 1000ull);
    auto result = cli.Get(req.path.c_str());
    if (result) {
      res.status = result->status;
      res.set_content(result->body, result->get_header_value("Content-Type"));
    } else {
      res.status = 503;
      res.set_content("{\"error\": \"Backend unavailable\"}", "application/json");
    }
    });

  svr.Post("/api/.*", [&prefs](const httplib::Request &req, httplib::Response &res) {
    LOG_START;
    LOG_MSG << "svr.Post" << req.method << req.path;

    std::string contentType = req.get_header_value("Content-Type");
    if (contentType.empty()) {
      contentType = "application/json";
    }

    // Special case for /api/chat - handle streaming
    if (req.path.find("/api/chat") != std::string::npos) {

      // Set up streaming response headers
      res.set_header("Content-Type", "text/event-stream");
      res.set_header("Cache-Control", "no-cache");
      res.set_header("Connection", "keep-alive");

      res.set_chunked_content_provider(
        "text/event-stream",
        [&prefs, req, &res, contentType](size_t offset, httplib::DataSink &sink) {
          LOG_MSG << "Starting chunked content provider, offset:" << offset;
          std::string host;
          int port;
          {
            std::lock_guard<std::mutex> lock(prefs.mutex_);
            host = prefs.host;
            port = prefs.port;
          }

          httplib::Client cli(host, port);
          cli.set_connection_timeout(0, 60 * 1000ull);

          httplib::Headers headers = { {"Accept", "text/event-stream"} };
          auto postRes = cli.Post(
            req.path.c_str(),
            headers,
            req.body,
            contentType,
            [&sink](const char *data, size_t len) -> bool {
              //LOG_MSG << "Received chunk: " << len << " bytes";
              return sink.write(data, len);
            }
          );

          sink.done();
          
          if (!postRes) {
            LOG_MSG << "Error: Backend streaming unavailable";
            res.status = 503;
            res.set_content("{\"error\": \"Backend streaming unavailable\"}", "application/json");
            return false;
          }
          if (postRes->status != 200) {
            LOG_MSG << "Error: Backend streaming returned status" << postRes->status;
            res.status = postRes->status;
          }
          
          LOG_MSG << "Streaming completed successfully";

          return true;
        });
      
    } else {
      // Regular POST handling for non-streaming endpoints
      std::string host;
      int port;
      {
        std::lock_guard<std::mutex> lock(prefs.mutex_);
        host = prefs.host;
        port = prefs.port;
      }

      httplib::Client cli(host, port);
      cli.set_connection_timeout(0, 60 * 1000ull);

      auto result = cli.Post(req.path.c_str(), req.body, contentType);

      if (result) {
        res.status = result->status;
        res.set_content(result->body, result->get_header_value("Content-Type"));
      } else {
        res.status = 503;
        res.set_content("{\"error\": \"Backend unavailable\"}", "application/json");
      }
    }
    });

  std::atomic<bool> serverReady{ false };

  const int serverPort = svr.bind_to_any_port("127.0.0.1");
  std::thread serverThread([&svr, &serverReady, serverPort]() {
    LOG_START;
    LOG_MSG << "Starting HTTP server on http://127.0.0.1:" << serverPort;
    serverReady = true;
    svr.listen_after_bind();
    LOG_MSG << "HTTP server stopped";
    });

  // Wait for server to be ready
  while (!serverReady.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  try {
    LOG_MSG << "Using window size, w" << prefs.width << ", h" << prefs.height;


    Webview w(
#ifdef _DEBUG
      true
#else
      true // for now
#endif
      , nullptr);
    w.setAppIcon("logo");
    w.set_title(fmt::format("PhenixCode Assistant - v1.0 [build date: {} {}]", __DATE__, __TIME__));
    w.set_size(prefs.width, prefs.height, WEBVIEW_HINT_NONE);
    w.onDestroyCallback_ = [&w, &prefs]
      {
        auto [width, height] = w.getWindowSize();
        LOG_MSG << "Saving window size [" << LOG_NOSPACE << width << ", " << height << "]";
        {
          std::lock_guard<std::mutex> lock(prefs.mutex_);
          prefs.width = width;
          prefs.height = height;
          savePrefsToFile(prefs);
        }
      };
#ifdef _WIN32
    HWND hWnd = static_cast<HWND>(w.window().value());
    WinDarkTitlebarImpl winDarkImpl;
    winDarkImpl.init();
    auto changeTheme = [&winDarkImpl, hWnd](bool dark) {
      winDarkImpl.setTitleBarTheme(hWnd, dark);
      };
    changeTheme(prefs.uiPrefs["darkOrLight"] == "dark");
#else
    auto changeTheme = [](bool) {};
#endif

    w.bind("setPersistentKey", [&prefs, &svr, changeTheme](const std::string &id, const std::string &data, void *)
      {
        LOG_MSG << "setPersistentKey:" << id << data;
        try {
          auto j = nlohmann::json::parse(data);
          if (j.is_array() && 2 == j.size()) {
            std::string key = j[0];
            std::string val = j[1];
            LOG_MSG << key << val;
            if (!key.empty()) {
              std::lock_guard<std::mutex> lock(prefs.mutex_);
              prefs.uiPrefs[key] = val;
              savePrefsToFile(prefs);
              LOG_MSG << "Saved persistent key:" << key;
#ifdef _WIN32
              if (key == "darkOrLight") {
                changeTheme(val == "dark");
              }
#endif
              return;
            }
          }
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
        }
      }, nullptr
    );

    w.bind("getPersistentKey", [&prefs, &svr](const std::string &data) -> std::string
      {
        LOG_MSG << "getPersistentKey:" << data;
        try {
          auto j = nlohmann::json::parse(data);
          if (j.is_array() && 0 < j.size()) {
            std::string key = j[0].get<std::string>();
            std::lock_guard<std::mutex> lock(prefs.mutex_);
            auto it = prefs.uiPrefs.find(key);
            if (it != prefs.uiPrefs.end()) {
              return nlohmann::json(it->second).dump();
            }
          }
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
        }
        return "null";
      }
    );

    w.bind("setServerUrl", [&prefs, &svr](const std::string &url) -> std::string
      {
        LOG_MSG << "setServerUrl:" << url;
        try {
          size_t hostStart = url.find("://") + 3;
          size_t portStart = url.find(":", hostStart);
          size_t pathStart = url.find("/", hostStart);
          std::lock_guard<std::mutex> lock(prefs.mutex_);
          std::string newHost;
          int newPort = prefs.port;
          if (portStart != std::string::npos) {
            newHost = url.substr(hostStart, portStart - hostStart);
            std::string portStr = url.substr(portStart + 1, pathStart - portStart - 1);
            newPort = std::stoi(portStr);
          } else {
            newHost = url.substr(hostStart, pathStart - hostStart);
          }
          if (newHost == "localhost") newHost = "127.0.0.1";
          prefs.host = newHost;
          prefs.port = newPort;
          savePrefsToFile(prefs);
          return "{\"status\": \"success\", \"message\": \"Server connection updated\"}";
        } catch (const std::exception &e) {
          LOG_MSG << "Error updating server connection:" << e.what();
          return "{\"status\": \"error\", \"message\": \"" + std::string(e.what()) + "\"}";
        }
      }
    );

    w.bind("getServerUrl", [&prefs, &svr](const std::string &) -> std::string
      {
        std::lock_guard<std::mutex> lock(prefs.mutex_);
        LOG_MSG << "getServerUrl" << prefs.host << prefs.port;
        try {
          std::string url = fmt::format("http://{}:{}", prefs.host, prefs.port);
          return nlohmann::json(url).dump();
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
        }
        return "null";
      }
    );

    w.bind("getSettingsFileProjectId", [](const std::string &data) -> std::string
      {
        LOG_MSG << "getSettingsFileProjectId";
        try {
          auto j = nlohmann::json::parse(data);
          if (j.is_array() && 0 < j.size()) {
            auto id = getProjectId(j[0]);
            LOG_MSG << "  \"" << LOG_NOSPACE << id << "\"";
            return nlohmann::json(id).dump();
          }
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
        }
        return "null";
      }
    );

    w.bind("startEmbedder", [&procUtil](const std::string &data) -> std::string
      {
        LOG_MSG << "startEmbedder:" << data;
        nlohmann::json res;
        try {
          auto j = nlohmann::json::parse(data);
          if (j.is_array() && 1 < j.size()) {
            std::string exePath = j[0].get<std::string>();
            std::string configPath = j[1].get<std::string>();
            if (!std::filesystem::exists(exePath))
              throw std::runtime_error("Embedder executable not found: " + exePath);
            if (!std::filesystem::exists(configPath))
              throw std::runtime_error("Embedder config file not found: " + configPath);
            auto appKey = generateAppKey();
            auto projectId = getProjectId(configPath);
            auto proc = procUtil.getOrCreateProcess(appKey, projectId);
            assert(proc);
            if (proc->startProcess(exePath, { "--config", configPath, "serve", "--appkey", appKey })) {
              res["status"] = "success";
              res["message"] = "Embedder started successfully";
              res["projectId"] = projectId;
              res["appKey"] = appKey; // use to id proc
              LOG_MSG << "Started embedder process" << proc->getProcessId() << "for projectId" << projectId;
            } else {
              procUtil.discardProcess(appKey);
              throw std::runtime_error("Failed to start embedder process");
            }
          } else {
            throw std::runtime_error("Invalid parameters for startEmbedder");
          }
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
          res["status"] = "error";
          res["message"] = ex.what();
        }
        return res.dump();
      }
    );

    w.bind("stopEmbedder", [&prefs, &procUtil](const std::string &data) -> std::string
      {
        LOG_MSG << "stopEmbedder:" << data;
        nlohmann::json res;
        try {
          auto j = nlohmann::json::parse(data);
          if (j.is_array() && 2 < j.size()) {
            const std::string appKey = j[0].get<std::string>();
            auto proc = procUtil.getProcessWithApiKey(appKey);
            if (!proc)
              throw std::runtime_error("Embedder appKey not found: " + appKey);
            std::string host = j[1].get<std::string>();
            if (host.empty())
              throw std::runtime_error("Invalid host for embedder shutdown");
            const int port = j[2].get<int>();
            if (port <= 0)
              throw std::runtime_error("Invalid port for embedder shutdown");
            if (host == "localhost") host = "127.0.0.1";
            assert(proc);
            httplib::Client cli(host, port);
            httplib::Headers headers = { {"X-App-Key", appKey} };
            auto result = cli.Post("/api/shutdown", headers, "", "application/json");
            if (result && result->status == 200) {
              LOG_MSG << "Shutdown request sent to embedder process" << proc->getProcessId();
            } else {
              LOG_MSG << "Failed to send shutdown request to embedder process" << proc->getProcessId();
            }
            if (proc->waitForCompletion(10000)) {
              LOG_MSG << "Embedder process" << proc->getProcessId() << "exited cleanly";
            } else {
              LOG_MSG << "Embedder process" << proc->getProcessId() << "did not exit in time, terminating...";
              proc->stopProcess();
            }
            procUtil.discardProcess(appKey);
            res["status"] = "success";
            res["message"] = "Embedder stopped successfully";
          } else {
            throw std::runtime_error("Invalid parameters for startEmbedder");
          }
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
          res["status"] = "error";
          res["message"] = ex.what();
        }
        return res.dump();
      }
    );
    
    w.init(R"(
      window.cppApi = {
        setServerUrl,
        getServerUrl,
        setPersistentKey,
        getPersistentKey,
        getSettingsFileProjectId,
        startEmbedder,
        stopEmbedder,
      };
      window.addEventListener('error', function(e) {
        console.error('JS Error:', e.message, e.filename, e.lineno);
      });
      console.log('Webview initialized, location:', window.location.href);
    )");

    const std::string url = "http://127.0.0.1:" + std::to_string(serverPort);
    LOG_MSG << "Navigating to:" << url;

    w.navigate(url);
    w.run();

    LOG_MSG << "Webview closed by user.";

    LOG_MSG << "Stopping HTTP server...";
  } catch (const std::exception &e) {
    LOG_MSG << "Webview error:" << e.what();
  }
  
  // Graceful shutdown of self-started processes
  {
    std::string host;
    int port;
    {
      std::lock_guard<std::mutex> lock(prefs.mutex_);
      host = prefs.host;
      port = prefs.port;
    }
    httplib::Client cli(host, port);
    auto result = cli.Get("/api/instances");
    if (result && result->status == 200) {
      try {
        auto j = nlohmann::json::parse(result->body);
        if (j.is_object()) {
          j = j["instances"];
          for (const auto &item : j) {
            if (item.contains("project_id") && item["project_id"].is_string()) {
              std::string project_id = item["project_id"].get<std::string>();
              std::string host = item.value("host", "");
              int port = item.value("port", 0);
              if (host.empty() || port <= 0) {
                LOG_MSG << "Invalid host/port for instance with project_id:" << project_id;
                continue;
              }
              std::string appKey = procUtil.getApiKeyFromProjectId(project_id);
              if (appKey.empty()) {
                LOG_MSG << "Embedder process" << project_id << "not started by this client. Skipped.";
                continue;
              }
              if (host == "localhost") host = "127.0.0.1";
              httplib::Client cli(host, port);
              httplib::Headers headers = { {"X-App-Key", appKey} };
              auto result = cli.Post("/api/shutdown", headers, "", "application/json");
              if (result && result->status == 200) {
                LOG_MSG << "Shutdown request sent to embedder process for project_id:" << project_id;
              } else {
                LOG_MSG << "Failed to send shutdown request to embedder process for project_id:" << project_id;
              }
            }
          }
        }
      } catch (const std::exception &e) {
        LOG_MSG << "Error parsing /api/instances response:" << e.what();
      }
    } else {
      LOG_MSG << "Failed to query /api/instances";
    }
    procUtil.waitToStopThenTerminate();
  }

  svr.stop();
  if (serverThread.joinable()) {
    serverThread.join();
    LOG_MSG << "HTTP server thread joined cleanly";
  }

  return 0;
}