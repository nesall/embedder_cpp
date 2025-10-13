#pragma once

#include <memory>
#include <string>

class Chunker;
class Settings;
class VectorDatabase;
class SourceProcessor;
class EmbeddingClient;
class CompletionClient;
class SimpleTokenCounter;

class App {
  struct Impl;
  std::unique_ptr<Impl> imp;
public:
  explicit App(const std::string &configPath);
  ~App();

  // CLI commands
  void embed();
  void watch(int interval_seconds = 60);
  size_t update();
  void compact();
  void search(const std::string &query, size_t topK = 5);
  void stats();
  void clear();
  void chat();
  void serve(int port, bool watch = false, int interval = 60);

  const Settings &settings() const;
  const SimpleTokenCounter &tokenizer() const;
  const SourceProcessor &sourceProcessor() const;
  const Chunker &chunker() const;
  const VectorDatabase &db() const;
  VectorDatabase &db();

public:
  static void printUsage();
  static int run(int argc, char *argv[]);

private:
  void initialize();
};

namespace utils {
  std::string currentTimestamp();
  time_t getFileModificationTime(const std::string &path);
  int safeStoI(const std::string &s, int def = 0);
  std::string trimmed(std::string_view sv);
}



// Author: Arman Sahakyan
//#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <string>
#include <string_view>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <thread>
#include <atomic>
#include <format>
#include <limits>

#ifdef QT_CORE_LIB
#include <QString>
#include <QStringList>
#include <QDebug>
#endif // QT_CORE_LIB

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif


namespace utils {

  // ============================================================================
  //                                Log
  // ============================================================================
  class Log {
  public:
    explicit Log(bool toFile = true)
      : toFile_(toFile) {
    }

    ~Log() { commit(); }

    template <typename T>
    Log &operator<<(T &&val) {
      if (hasLog_) ss_ << ' ';
      ss_ << std::forward<T>(val);
      hasLog_ = true;
      return *this;
    }

    Log &operator<<(std::string_view sv) {
      if (hasLog_) ss_ << ' ';
      ss_ << sv;
      hasLog_ = true;
      return *this;
    }

    Log &noquote() { return *this; }

    void commit() {
      if (!hasLog_) return;
      const std::string msg = ss_.str();
      ss_.str({});
      ss_.clear();
      hasLog_ = false;

      const auto line = std::format("[{}] tid={} \"{}\"", dateTime(), threadId(), msg);

      std::scoped_lock lock(globalMutex());
      if (toFile_) {
        ensureFileOpen();
        if (fout_.good()) {
          fout_ << line << '\n';
          fout_.flush();
        }
      }

      // Console log (always)
#ifdef QT_CORE_LIB
      qDebug().nospace().noquote() << "[" << dateTime().c_str() << "] tid=" << utils::cur_thread_id() << m.c_str() << " \"" << data.c_str() << "\"";
#else
      std::cout << msg << std::endl;
#ifdef _MSC_VER
      ::OutputDebugStringA((msg + "\n").c_str());
#endif // _MSC_VER
#endif // QT_CORE_LIB
    }

    static void terminate() {
      std::scoped_lock lock(globalMutex());
      if (fout_.is_open()) fout_.close();
    }

  private:
    bool toFile_;
    bool hasLog_ = false;
    std::ostringstream ss_;

    static inline std::ofstream fout_;
    static inline std::atomic<bool> initialized_ = false;

    static inline std::mutex &globalMutex() {
      static std::mutex m;
      return m;
    }

    static std::string dateTime() {
      using namespace std::chrono;
      const auto now = system_clock::now();
      const auto t = system_clock::to_time_t(now);
      std::tm tm{};
#ifdef _WIN32
      localtime_s(&tm, &t);
#else
      localtime_r(&t, &tm);
#endif
      std::ostringstream oss;
      oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
      return oss.str();
    }

    static uint64_t threadId() {
      auto id = std::this_thread::get_id();
      std::ostringstream oss;
      oss << id;
      return std::hash<std::string>{}(oss.str());
    }

    static void ensureFileOpen() {
      if (!initialized_) {
        const std::string fname = "output.log";
        rotateIfTooLarge(fname, 5 * 1024 * 1024);
        fout_.open(fname, std::ios::app);
        initialized_ = true;
      } else if (!fout_.is_open()) {
        fout_.open("output.log", std::ios::app);
      }
    }

    static void rotateIfTooLarge(const std::string &fname, uintmax_t maxSize) {
      namespace fs = std::filesystem;
      if (fs::exists(fname) && fs::file_size(fname) > maxSize) {
        const auto backup = fname + ".old";
        if (fs::exists(backup)) fs::remove(backup);
        fs::rename(fname, backup);
      }
    }
  };

#define LOG_MSG utils::Log()
#define LOG_MSGNF utils::Log(false)


  // ============================================================================
  //                            ScopeLogger (diagnostics.log)
  // ============================================================================
  class ScopeLogger {
  public:
    ScopeLogger(std::string_view func, std::string_view file, int line)
      : func_(func), file_(file), line_(line) {
      init();
      log("start...");
      count_ ++;
    }

    ScopeLogger(std::string_view func, std::string_view name, std::string_view file, int line)
      : func_(std::format("{}:{}", func, name)), file_(file), line_(line) {
      init();
      log("start...");
      count_++;
    }

    ~ScopeLogger() {
      count_--;
      log("end!");
    }

    void here(std::string_view msg) { log(msg); }

  private:
    std::string func_;
    std::string file_;
    int line_;

    inline static std::atomic<int> count_{ 0 };
    inline static std::ofstream fout_;
    inline static std::mutex mutex_;
    inline static bool initialized_ = false;
    inline static bool crashedLastTime_ = false;
    inline static bool crashChecked_ = false;

    static std::string dateTime() {
      using namespace std::chrono;
      const auto now = system_clock::now();
      const auto t = system_clock::to_time_t(now);
      std::tm tm{};
#ifdef _WIN32
      localtime_s(&tm, &t);
#else
      localtime_r(&t, &tm);
#endif
      std::ostringstream oss;
      oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
      return oss.str();
    }

    static std::string fileName() { return "diagnostics.log"; }

    static void rotateIfTooLarge(const std::string &fname, uintmax_t maxSize) {
      namespace fs = std::filesystem;
      if (fs::exists(fname) && maxSize < fs::file_size(fname)) {
        const auto backup = fname + ".old";
        if (fs::exists(backup)) fs::remove(backup);
        fs::rename(fname, backup);
      }
    }

    static std::string lastLine(const std::string &fname) {
      std::ifstream ifs(fname);
      if (!ifs.is_open()) return {};
      std::string line, last;
      while (std::getline(ifs, line)) last = line;
      return last;
    }

    static bool detectPreviousCrash() {
      if (crashChecked_) return crashedLastTime_;
      crashChecked_ = true;

      const auto fname = fileName();
      if (!std::filesystem::exists(fname)) return false;

      const auto line = lastLine(fname);
      const auto pos = line.rfind('|');
      if (pos == std::string::npos) return false;

      try {
        int n = std::stoi(line.substr(pos + 1));
        crashedLastTime_ = (n > 0);
      } catch (...) {
        crashedLastTime_ = false;
      }
      return crashedLastTime_;
    }

    static void ensureFileOpen() {
      if (!initialized_) {
        const std::string fname = fileName();
        rotateIfTooLarge(fname, 2ull * 1024 * 1024);
        fout_.open(fname, std::ios::app);
        initialized_ = true;

        if (detectPreviousCrash()) {
          fout_ << "## CRASH POINT ##\n";
          fout_.flush();
        }
      } else if (!fout_.is_open()) {
        fout_.open(fileName(), std::ios::app);
      }
    }

    void init() {
      std::scoped_lock lock(mutex_);
      ensureFileOpen();
    }

    void log(std::string_view phase) const {
      std::scoped_lock lock(mutex_);
      ensureFileOpen();
      const auto msg = std::format("[{}] {}:{} {} |{}\n", dateTime(), func_, phase, file_, count_.load());
      if (fout_.good()) {
        fout_ << msg;
        fout_.flush();
      }
    }
  };

  // Macros
#define LOG_START utils::ScopeLogger _scopelog_(__FUNCTION__, __FILE__, __LINE__)
#define LOG_START1(x) utils::ScopeLogger _scopelog_(__FUNCTION__, x, __FILE__, __LINE__)
#define LOG_HERE(x) _scopelog_.here(x)

} // namespace utils
