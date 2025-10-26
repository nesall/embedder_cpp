#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <vector>
#include <map>
#include "nlohmann/json.hpp"

struct ApiConfig {
  std::string id;
  std::string name;
  std::string apiUrl;
  std::string apiKey;
  std::string model;
  std::string queryFormat;
  std::string documentFormat;
  std::string maxTokensName; // e.g. max_tokens or max_completion_tokens
  bool temperatureSupport = true;
  bool stream = true;
  struct {
    float input = 0;
    float output = 0;
    float cachedInput = 0;
  } pricing;

  // Compute an effective "combined" price per million tokens.
  // hitRatio = fraction of input tokens served from cache (0.0–1.0)
double combinedPrice(double hitRatio = 0.05) const {
    // Only input can be cached; output is always fully billed
    double effectiveInput = pricing.input;
    if (0 < pricing.cachedInput) {
        effectiveInput = hitRatio * pricing.cachedInput + (1.0 - hitRatio) * pricing.input;
    }
    return effectiveInput + pricing.output;
}

};

class Settings {
private:
  nlohmann::json config_;
  std::string path_;

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

public:
  Settings(const std::string &path = "settings.json");

  void updateFromConfig(const nlohmann::json &config);
  void save();
  std::string configPath() const { return path_; }

  std::string tokenizerConfigPath() const {
    return config_["tokenizer"].value("config_path", "tokenizer.json");
  }

  int chunkingMaxTokens() const { return config_["chunking"].value("nof_max_tokens", 500); }
  int chunkingMinTokens() const { return config_["chunking"].value("nof_min_tokens", 50); }
  float chunkingOverlap() const { return config_["chunking"].value("overlap_percentage", 0.1f); }
  bool chunkingSemantic() const { return config_["chunking"].value("semantic", false); }

  ApiConfig embeddingCurrentApi() const;
  std::vector<ApiConfig> embeddingApis() const;
  size_t embeddingTimeoutMs() const { return config_["embedding"].value("timeout_ms", size_t(10'000)); }
  size_t embeddingBatchSize() const { return config_["embedding"].value("batch_size", size_t(4)); }
  size_t embeddingTopK() const { return config_["embedding"].value("top_k", size_t(5)); }

  ApiConfig generationCurrentApi() const;
  std::vector<ApiConfig> generationApis() const;
  size_t generationTimeoutMs() const { return config_["generation"].value("timeout_ms", size_t(20'000)); }
  size_t generationMaxFullSources() const { return config_["generation"].value("max_full_sources", size_t(2)); }
  size_t generationMaxRelatedPerSource() const { return config_["generation"].value("max_related_per_source", size_t(3)); }
  size_t generationMaxContextTokens() const { return config_["generation"].value("max_context_tokens", size_t(20'000)); }
  size_t generationMaxChunks() const { return config_["generation"].value("max_chunks", size_t(5)); }
  float generationDefaultTemperature() const { return config_["generation"].value("default_temperature", 0.5f); }
  size_t generationDefaultMaxTokens() const { return config_["generation"].value("default_max_tokens", size_t(2048)); }

  std::string databaseSqlitePath() const { return config_["database"].value("sqlite_path", "db.sqlite"); }
  std::string databaseIndexPath() const { return config_["database"].value("index_path", "index"); }
  size_t databaseVectorDim() const { return config_["database"].value("vector_dim", size_t(768)); }
  size_t databaseMaxElements() const { return config_["database"].value("max_elements", size_t(100'000)); }
  std::string databaseDistanceMetric() const { return config_["database"].value("distance_metric", "cosine"); }

  size_t filesMaxFileSizeMb() const { return config_["source"].value("max_file_size_mb", size_t(10)); }
  std::string filesEncoding() const { return config_["source"].value("encoding", "utf-8"); }
  std::vector<std::string> filesGlobalExclusions() const { return config_["source"].value("global_exclude", std::vector<std::string>{}); }
  std::vector<std::string> filesDefaultExtensions() const { return config_["source"].value("default_extensions", std::vector<std::string>{".txt", ".md"}); }

  std::string loggingLoggingFile() const {
    return config_.contains("logging") ? config_["logging"].value("logging_file", "output.log") : std::string("output.log");
  }
  std::string loggingDiagnosticsFile() const { 
    return config_.contains("logging") ? config_["logging"].value("diagnostics_file", "diagnostics.log") : std::string("diagnostics.log");
  }

  std::vector<SourceItem> sources() const;
  std::string configDump() const { return config_.dump(2); }
};


#endif // _SETTINGS_H_
