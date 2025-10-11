#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <string>
#include <vector>
#include <map>
#include "nlohmann/json.hpp"


class Settings {
private:
  nlohmann::json config_;

public:
  struct SourceItem {
    std::string type; // "directory", "file", "url"
    std::string path; // for "directory" and "file"
    bool recursive = true; // for "directory"
    std::vector<std::string> extensions; // for "directory"
    std::vector<std::string> exclude; // for "directory"
    std::string url; // for "url"
    std::map<std::string, std::string> headers; // for "url"
    std::size_t urlTimeoutMs = 10000; // default 10s
  };

  struct ApiConfig {
    std::string id;
    std::string name;
    std::string apiUrl;
    std::string apiKey;
    std::string model;
  };

public:
  Settings(const std::string &path = "settings.json");

  std::string tokenizerConfigPath() const {
    return config_["tokenizer"].value("config_path", "tokenizer.json");
  }

  int chunkingMaxTokens() const { return config_["chunking"].value("nof_max_tokens", 500); }
  int chunkingMinTokens() const { return config_["chunking"].value("nof_min_tokens", 50); }
  float chunkingOverlap() const { return config_["chunking"].value("overlap_percentage", 0.1f); }
  bool chunkingSemantic() const { return config_["chunking"].value("semantic", false); }

  Settings::ApiConfig embeddingCurrentApi() const;
  std::vector<Settings::ApiConfig> embeddingApis() const;
  size_t embeddingTimeoutMs() const { return config_["embedding"].value("timeout_ms", size_t(10'000)); }
  size_t embeddingBatchSize() const { return config_["embedding"].value("batch_size", size_t(16)); }
  size_t embeddingTopK() const { return config_["embedding"].value("top_k", size_t(5)); }

  Settings::ApiConfig generationCurrentApi() const;
  std::vector<Settings::ApiConfig> generationApis() const;
  size_t generationTimeoutMs() const { return config_["generation"].value("timeout_ms", size_t(20'000)); }
  size_t generationMaxFullSources() const { return config_["generation"].value("max_full_sources", size_t(2)); }
  size_t generationMaxRelatedPerSource() const { return config_["generation"].value("max_related_per_source", size_t(3)); }
  size_t generationMaxContextTokens() const { return config_["generation"].value("max_context_tokens", size_t(20'000)); }
  size_t generationMaxChunks() const { return config_["generation"].value("max_chunks", size_t(5)); }
  size_t generationDefaultTemperature() const { return config_["generation"].value("default_temperature", size_t(0.5)); }

  std::string databaseSqlitePath() const { return config_["database"].value("sqlite_path", "db.sqlite"); }
  std::string databaseIndexPath() const { return config_["database"].value("index_path", "index"); }
  size_t databaseVectorDim() const { return config_["database"].value("vector_dim", size_t(768)); }
  size_t databaseMaxElements() const { return config_["database"].value("max_elements", size_t(100'000)); }
  std::string databaseDistanceMetric() const { return config_["database"].value("distance_metric", "cosine"); }

  size_t filesMaxFileSizeMb() const { return config_["files"].value("max_file_size_mb", size_t(10)); }
  std::string filesEncoding() const { return config_["files"].value("encoding", "utf-8"); }
  std::vector<std::string> filesGlobalExclusions() const { return config_["files"].value("global_exclude", std::vector<std::string>{}); }
  std::vector<std::string> filesDefaultExtensions() const { return config_["files"].value("default_extensions", std::vector<std::string>{".txt", ".md"}); }

  std::vector<SourceItem> sources() const;
};


#endif // _SETTINGS_H_
