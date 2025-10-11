#include "app.h"
#include "settings.h"
#include "database.h"
#include "inference.h"
#include "chunker.h"
#include "tokenizer.h"
#include "sourceproc.h"
#include "httpserver.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <unordered_map>
#include <thread>
#include <stdexcept>
#include <chrono>
#include <ranges>
#include <cctype>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;



std::string utils::currentTimestamp()
{
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

time_t utils::getFileModificationTime(const std::string &path)
{
  auto ftime = fs::last_write_time(path);
  auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
  return std::chrono::system_clock::to_time_t(sctp);
}

int utils::safeStoI(const std::string &s, int def)
{
  try {
    return std::stoi(s);
  } catch (...) {
  }
  return def;
}

std::string utils::trimmed(std::string_view sv)
{
  auto wsfront = std::find_if_not(sv.begin(), sv.end(), ::isspace);
  auto wsback = std::find_if_not(sv.rbegin(), sv.rend(), ::isspace).base();
  return (wsfront < wsback ? std::string(wsfront, wsback) : std::string{});
}


namespace {

  class IncrementalUpdater {
  private:
    VectorDatabase *db_;

  public:
    IncrementalUpdater(VectorDatabase *db) : db_(db) {
    }

    ~IncrementalUpdater() {
    }

    struct UpdateInfo {
      std::vector<std::string> newFiles;
      std::vector<std::string> modifiedFiles;
      std::vector<std::string> deletedFiles;
      std::vector<std::string> unchangedFiles;
    };

    UpdateInfo detectChanges(const std::vector<std::string> &currentFiles) {
      UpdateInfo info;
      auto trackedFiles = db_->getTrackedFiles();
      std::unordered_map<std::string, FileMetadata> trackedMap;
      for (const auto &meta : trackedFiles) {
        trackedMap[meta.path] = meta;
      }
      for (const auto &filepath : currentFiles) {
        if (!fs::exists(filepath)) continue;
        auto currentModTime = utils::getFileModificationTime(filepath);
        auto currentSize = fs::file_size(filepath);
        auto it = trackedMap.find(filepath);
        if (it == trackedMap.end()) {
          info.newFiles.push_back(filepath);
        } else {
          if (it->second.lastModified != currentModTime || it->second.fileSize != currentSize) {
            info.modifiedFiles.push_back(filepath);
          } else {
            info.unchangedFiles.push_back(filepath);
          }
          trackedMap.erase(it);
        }
      }

      // Remaining files in trackedMap are deleted
      for (const auto &[path, _] : trackedMap) {
        info.deletedFiles.push_back(path);
      }

      return info;
    }

    bool needsUpdate(const UpdateInfo &info) {
      return !info.newFiles.empty() || !info.modifiedFiles.empty() || !info.deletedFiles.empty();
    }

    // Update database incrementally
    size_t updateDatabase(EmbeddingClient &client, Chunker &chunker, const UpdateInfo &info) {
      size_t totalUpdated = 0;

      if (!info.deletedFiles.empty()) {
        try {
          db_->beginTransaction();
          for (const auto &filepath : info.deletedFiles) {
            std::cout << "Deleting chunks for: " << filepath << std::endl;
            db_->deleteDocumentsBySource(filepath);
            db_->removeFileMetadata(filepath);
            totalUpdated++;
          }
          db_->commit();
        } catch (const std::exception &e) {
          db_->rollback();
          std::cerr << "  Error during deletions: " << e.what() << std::endl;
          return totalUpdated;
        }
      }

      // Handle modifications (delete old, insert new)
      for (const auto &filepath : info.modifiedFiles) {
        std::cout << "Updating: " << filepath << std::endl;

        try {
          db_->beginTransaction();
          db_->deleteDocumentsBySource(filepath);

          std::string content = readFile(filepath);
          auto chunks = chunker.chunkText(content, filepath);

          for (const auto &chunk : chunks) {
            std::vector<float> embedding;
            client.generateEmbeddings({ chunk.text }, embedding);
            db_->addDocument(chunk, embedding);
          }

          //updateFileMetadata(filepath);
          totalUpdated++;

          std::cout << "  Updated with " << chunks.size() << " chunks" << std::endl;

          db_->commit();

        } catch (const std::exception &e) {
          db_->rollback();
          std::cerr << "  Error: " << e.what() << std::endl;
        }
      }

      // Handle new files
      for (const auto &filepath : info.newFiles) {
        std::cout << "Adding new file: " << filepath << std::endl;

        try {
          db_->beginTransaction();

          std::string content = readFile(filepath);
          auto chunks = chunker.chunkText(content, filepath);

          for (const auto &chunk : chunks) {
            std::vector<float> embedding;
            client.generateEmbeddings({ chunk.text }, embedding);
            db_->addDocument(chunk, embedding);
          }

          totalUpdated++;

          std::cout << "  Added with " << chunks.size() << " chunks" << std::endl;
          db_->commit();
        } catch (const std::exception &e) {
          std::cerr << "  Error: " << e.what() << std::endl;
          db_->rollback();
        }
      }

      // Save index after all updates
      if (totalUpdated > 0) {
        db_->persist();
      }

      return totalUpdated;
    }

    // Print update summary
    void printUpdateSummary(const UpdateInfo &info) {
      std::cout << "\n=== Update Summary ===" << std::endl;
      std::cout << "New files: " << info.newFiles.size() << std::endl;
      std::cout << "Modified files: " << info.modifiedFiles.size() << std::endl;
      std::cout << "Deleted files: " << info.deletedFiles.size() << std::endl;
      std::cout << "Unchanged files: " << info.unchangedFiles.size() << std::endl;

      if (!info.newFiles.empty()) {
        std::cout << "\nNew:" << std::endl;
        for (const auto &file : info.newFiles) {
          std::cout << "  + " << file << std::endl;
        }
      }

      if (!info.modifiedFiles.empty()) {
        std::cout << "\nModified:" << std::endl;
        for (const auto &file : info.modifiedFiles) {
          std::cout << "  * " << file << std::endl;
        }
      }

      if (!info.deletedFiles.empty()) {
        std::cout << "\nDeleted:" << std::endl;
        for (const auto &file : info.deletedFiles) {
          std::cout << "  - " << file << std::endl;
        }
      }
    }

  private:
    std::string readFile(const std::string &path) {
      std::ifstream file(path);
      if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
      }
      std::stringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
    }
  };

} // anonymous namespace


struct App::Impl {
  std::unique_ptr<Settings> settings_;
  std::unique_ptr<VectorDatabase> db_;
  std::unique_ptr<EmbeddingClient> embeddingClient_;
  std::unique_ptr<CompletionClient> completionClient_;
  std::unique_ptr<SimpleTokenCounter> tokenizer_;
  std::unique_ptr<Chunker> chunker_;
  std::unique_ptr<SourceProcessor> processor_;
  std::unique_ptr<IncrementalUpdater> updater_;
  std::unique_ptr<HttpServer> httpServer_;
};

App::App(const std::string &configPath) : imp(new Impl)
{
  imp->settings_ = std::make_unique<Settings>(configPath);
  initialize();
}

App::~App()
{
  imp->httpServer_->stop();
}

void App::initialize()
{
  auto &ss = *imp->settings_;
  std::string dbPath = ss.databaseSqlitePath();
  std::string indexPath = ss.databaseIndexPath();
  size_t vectorDim = ss.databaseVectorDim();
  size_t maxElements = ss.databaseMaxElements();
  VectorDatabase::DistanceMetric metric = ss.databaseDistanceMetric() == "cosine" ? VectorDatabase::DistanceMetric::Cosine : VectorDatabase::DistanceMetric::L2;

  imp->db_ = std::make_unique<HnswSqliteVectorDatabase>(dbPath, indexPath, vectorDim, maxElements, metric);

  imp->embeddingClient_ = std::make_unique<EmbeddingClient>(ss);
  imp->completionClient_ = std::make_unique<CompletionClient>(*this);

  imp->tokenizer_ = std::make_unique<SimpleTokenCounter>(ss.tokenizerConfigPath());

  size_t minTokens = ss.chunkingMinTokens();
  size_t maxTokens = ss.chunkingMaxTokens();
  float overlap = ss.chunkingOverlap();

  imp->chunker_ = std::make_unique<Chunker>(*imp->tokenizer_, minTokens, maxTokens, overlap);
  imp->processor_ = std::make_unique<SourceProcessor>(*imp->settings_);
  imp->updater_ = std::make_unique<IncrementalUpdater>(imp->db_.get());

  imp->httpServer_ = std::make_unique<HttpServer>(*this);
}

void App::embed()
{
  std::cout << "Starting embedding process..." << std::endl;
  auto sources = imp->processor_->collectSources();
  size_t totalChunks = 0;
  size_t totalFiles = 0;
  size_t totalTokens = 0;
  for (size_t i = 0; i < sources.size(); ++i) {
    const auto &[text, source] = sources[i];
    try {
      std::cout << "PROCESSING " << source << std::endl;
      std::string sourceId = std::filesystem::path(source).string();
      auto chunks = imp->chunker_->chunkText(text, sourceId);
      std::cout << "  Generated " << chunks.size() << " chunks" << std::endl;
      size_t batchSize = imp->settings_->embeddingBatchSize();
      for (size_t i = 0; i < chunks.size(); i += batchSize) {
        size_t end = (std::min)(i + batchSize, chunks.size());
        std::vector<Chunk> batch(chunks.begin() + i, chunks.begin() + end);
        std::vector<std::vector<float>> embeddings;
        size_t iBatch = 1;
        for (const auto &chunk : batch) {
          std::cout << "GENERATING embeddings for batch " << iBatch++ << "/" << batch.size() << "\r" << std::flush;
          std::vector<float> emb;
          imp->embeddingClient_->generateEmbeddings({ chunk.text }, emb);
          embeddings.push_back(emb);
          totalTokens += chunk.metadata.tokenCount;
        }
        imp->db_->addDocuments(batch, embeddings);
        std::cout << "  Processed all chunks.                     \r" << std::flush;
      }
      std::cout << std::endl;
      totalChunks += chunks.size();
      totalFiles++;
    } catch (const std::exception &e) {
      std::cerr << "Error processing " << source << ": " << e.what() << std::endl;
    }
  }
  imp->db_->persist();
  std::cout << "\nCompleted!" << std::endl;
  std::cout << "  Files processed: " << totalFiles << std::endl;
  std::cout << "  Total chunks: " << totalChunks << std::endl;
  std::cout << "  Total tokens: " << totalTokens << std::endl;
}

void App::compact()
{
  std::cout << "Compacting vector index..." << std::endl;
  imp->db_->compact();
  imp->db_->persist();
  std::cout << "Done!" << std::endl;
}

void App::search(const std::string &query, size_t topK)
{
  std::cout << "Searching for: " << query << std::endl;

  std::vector<float> queryEmbedding;
  imp->embeddingClient_->generateEmbeddings({ query }, queryEmbedding);
  auto results = imp->db_->search(queryEmbedding, topK);

  std::cout << "\nFound " << results.size() << " results:" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  for (size_t i = 0; i < results.size(); ++i) {
    const auto &result = results[i];
    std::cout << "\n[" << (i + 1) << "] Score: " << result.similarityScore << std::endl;
    std::cout << "Source: " << result.sourceId << std::endl;
    std::cout << "Type: " << result.chunkType << std::endl;
    std::cout << "Content: " << result.content.substr(0, 200);
    if (result.content.length() > 200) std::cout << "...";
    std::cout << std::endl;
  }
}

void App::stats()
{
  auto s = imp->db_->getStats();

  std::cout << "\n=== Database Statistics ===" << std::endl;
  std::cout << "Total chunks: " << s.totalChunks << std::endl;
  std::cout << "Vectors in index: " << s.vectorCount << std::endl;

  std::cout << "\nChunks by source:" << std::endl;
  for (const auto &[source, count] : s.sources) {
    std::cout << "  " << source << ": " << count << std::endl;
  }

  //std::cout << "\nChunks by type:" << std::endl;
  //for (const auto &[type, count] : s.types) {
  //  std::cout << "  " << type << ": " << count << std::endl;
  //}
}

void App::clear()
{
  std::cout << "Are you sure you want to clear all data? (yes/no): ";
  std::string confirm;
  std::cin >> confirm;

  if (confirm == "yes") {
    imp->db_->clear();
    std::cout << "Database cleared." << std::endl;
  } else {
    std::cout << "Cancelled." << std::endl;
  }
}

void App::chat()
{
  std::cout << "Entering chat mode. Type 'exit' to quit." << std::endl;
  auto apiCfg = imp->settings_->generationCurrentApi();
  std::cout << "Using model: " << apiCfg.model << " at " << apiCfg.apiUrl << std::endl;
  std::vector<json> messages;
  messages.push_back({ {"role", "system"}, {"content", "You are a helpful assistant."} });
  while (true) {
    try {
      std::cout << "\nYou: ";
      std::string userInput;
      std::getline(std::cin, userInput);
      if (userInput == "exit") break;
      messages.push_back({ {"role", "user"}, {"content", userInput} });
      // Generate embedding for the user input
      std::vector<float> queryEmbedding;
      imp->embeddingClient_->generateEmbeddings({ userInput }, queryEmbedding);
      auto searchResults = imp->db_->search(queryEmbedding, 5);
      std::cout << "\nAssistant: " << std::flush;
      std::string assistantResponse = imp->completionClient_->generateCompletion(
        messages, searchResults, 0.0f,
        [](const std::string &chunk) {
          std::cout << chunk << std::flush;
        }
      );
      std::cout << std::endl;
      messages.push_back({ {"role", "assistant"}, {"content", assistantResponse} });
    } catch (const std::exception &e) {
      std::cout << "Error: " << e.what() << "\n";
    }
  }
  std::cout << "Exiting chat mode." << std::endl;
}

void App::serve(int port, bool watch, int interval)
{
  imp->httpServer_->startServer(port, watch, interval);
}

size_t App::update()
{
  std::cout << "Checking for changes..." << std::endl;
  auto sources = imp->processor_->collectSources();
  std::vector<std::string> currentFiles;
  for (const auto &source : sources) {
    currentFiles.push_back(source.source);
  }
  auto info = imp->updater_->detectChanges(currentFiles);
  imp->updater_->printUpdateSummary(info);
  if (!imp->updater_->needsUpdate(info)) {
    std::cout << "\nNo updates needed. Database is up to date." << std::endl;
    return 0;
  }
  std::cout << "\nApplying updates..." << std::endl;
  size_t updated = imp->updater_->updateDatabase(*imp->embeddingClient_, *imp->chunker_, info);
  std::cout << "\nUpdate completed! " << updated << " files processed." << std::endl;
  return updated;
}

void App::watch(int intervalSeconds)
{
  std::cout << "Starting watch mode (checking every " << intervalSeconds << " seconds)" << std::endl;
  std::cout << "Press Ctrl+C to stop" << std::endl;
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
    try {
      if (update()) {
        std::cout << "[" << utils::currentTimestamp() << "] updates detected and applied.\n";
      }
    } catch (const std::exception &e) {
      std::cerr << "Error during update: " << e.what() << std::endl;
    }
  }
}

const Settings &App::settings() const
{
  return *imp->settings_;
}

const SimpleTokenCounter &App::tokenizer() const
{
  return *imp->tokenizer_;
}

const SourceProcessor &App::sourceProcessor() const
{
  return *imp->processor_;
}

const Chunker &App::chunker() const
{
  return *imp->chunker_;
}

const VectorDatabase &App::db() const
{
  return *imp->db_;
}

VectorDatabase &App::db()
{
  return *imp->db_;
}

const EmbeddingClient &App::embeddingClient() const
{
  return *imp->embeddingClient_;
}

const CompletionClient &App::completionClient() const
{
  return *imp->completionClient_;
}

void App::printUsage()
{
  std::cout << "Usage: embedder <command> [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  embed              - Process and embed all configured sources\n";
  std::cout << "  update             - Incrementally update changed files only\n";
  std::cout << "  watch [seconds]    - Continuously monitor and update (default: 60s)\n";
  std::cout << "  search <query>     - Search for similar chunks\n";
  std::cout << "  stats              - Show database statistics\n";
  std::cout << "  clear              - Clear all data\n";
  std::cout << "  compact            - Reclaim deleted space\n";
  std::cout << "  chat               - Chat mode\n";
  std::cout << "  serve [options]    - Start HTTP API server\n";
  std::cout << "\nServe options:\n";
  std::cout << "  --port <port>      - Server port (default: 8081)\n";
  std::cout << "  --watch [seconds]  - Enable auto-update (default: 60s)\n";
  std::cout << "\nGeneral options:\n";
  std::cout << "  --config <path>    - Config file path (default: settings.json)\n";
  std::cout << "  --top <k>          - Number of results for search (default: 5)\n";
  std::cout << "\nExamples:\n";
  std::cout << "  embedder serve --port 8081 --watch 30\n";
  std::cout << "  embedder serve --watch    # Use defaults\n";
  std::cout << "  embedder watch 120    # Watch mode without server\n";
  std::cout << std::endl;

  std::cout << std::endl;
}

int App::run(int argc, char *argv[])
{
  try {
    if (argc < 2) {
      printUsage();
      return 1;
    }
    std::string command = argv[1];
    std::string configPath = "settings.json";
    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--config" && i + 1 < argc) {
        configPath = argv[++i];
      }
    }

    App app(configPath);

    if (false) {
    } else if (command == "embed") {
      app.embed();
    } else if (command == "update") {
      app.update();
    } else if (command == "watch") {
      int interval = 60;
      if (argc > 2) {
        interval = utils::safeStoI(argv[2], interval);
        std::cout << "Using interval " << interval << "s\n";
      }
      app.watch(interval);
    } else if (command == "search") {
      if (argc < 3) {
        std::cerr << "Error: search requires a query" << std::endl;
        return 1;
      }
      std::string query = argv[2];
      size_t topK = 5;
      for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--top" && i + 1 < argc) {
          topK = utils::safeStoI(argv[++i]);
        }
      }
      app.search(query, topK);
    } else if (command == "stats") {
      app.stats();
    } else if (command == "clear") {
      app.clear();
    } else if (command == "compact") {
      app.compact();
    } else if (command == "chat") {
      app.chat();
    } else if (command == "serve") {
      int port = 8081;
      bool enableWatch = false;
      int watchInterval = 60;
      for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
          port = utils::safeStoI(argv[++i], port);
        } else if (arg == "--watch") {
          enableWatch = true;
          if (i + 1 < argc && argv[i + 1][0] != '-') {
            watchInterval = utils::safeStoI(argv[++i], watchInterval);
          }
        }
      }
      app.serve(port, enableWatch, watchInterval);
    } else {
      std::cerr << "Unknown command: " << command << std::endl;
      printUsage();
      return 1;
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n\n";
    printUsage();
    return 1;
  }
  return 0;
}
