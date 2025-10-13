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
