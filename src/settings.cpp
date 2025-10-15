#include "settings.h"
#include <fstream>
#include <stdexcept>
#include <cstdlib>

namespace {
  // Simple ${VAR} substitution
  std::string expandEnvVar(const std::string var) {
    if (var.starts_with("${") && var.ends_with("}")) {
      std::string envVar = var.substr(2, var.length() - 3);
      if (const char *envValue = getenv(envVar.c_str())) {
        return std::string(envValue);
      }
    }
    return var;
  }

  void fetchApiConfigFromItem(const nlohmann::json &item, ApiConfig &cfg, const nlohmann::json &section) {
    cfg.id = item.value("id", "");
    cfg.name = item.value("name", "");
    cfg.apiUrl = item.value("api_url", item.value("apiUrl", ""));
    cfg.apiKey = expandEnvVar(item.value("api_key", item.value("apiKey", "")));
    cfg.model = item.value("model", "");
    cfg.maxTokensName = item.value("max_tokens_name", section.value("default_max_tokens_name", "max_tokens"));
    cfg.temperatureSupport = item.value("temperature_support", true);
    if (item.contains("pricing_tpm")) {
      auto pricing = item["pricing_tpm"];
      if (pricing.is_object()) {
        cfg.pricing.input = pricing.value("input", 0.f);
        cfg.pricing.output = pricing.value("output", 0.f);
        cfg.pricing.cachedInput = pricing.value("cached_input", 0.f);
      }
    }
  }

  std::vector<ApiConfig> getApiConfigList(const nlohmann::json &section) {
    std::vector<ApiConfig> v;
    if (!section.contains("apis") || !section["apis"].is_array()) return v;
    for (const auto &item : section["apis"]) {
      if (!item.is_object()) continue;
      ApiConfig cfg;
      fetchApiConfigFromItem(item, cfg, section);
      v.push_back(cfg);
    }
    return v;
  }

  ApiConfig getCurrentApiConfig(const nlohmann::json &section) {
    ApiConfig cfg;
    if (!section.is_object()) return cfg;
    std::string current = section.value("current_api", "");
    if (!section.contains("apis") || !section["apis"].is_array()) return cfg;
    for (const auto &item : section["apis"]) {
      if (!item.is_object()) continue;
      std::string id = item.value("id", "");
      if (current.empty() || id == current) {
        fetchApiConfigFromItem(item, cfg, section);
        return cfg;
      }
    }
    if (!section["apis"].empty()) {
      auto &api = section["apis"][0];
      return {
        api.value("id", ""),
        api.value("name", ""),
        api.value("api_url", ""),
        expandEnvVar(api.value("api_key", "")),
        api.value("model", "")
      };
    }
    return cfg;
  }
} // anonymous namespace

Settings::Settings(const std::string &path)
{
  std::ifstream file(path);
  if (!file.is_open()) {
    file.open("../" + path);
    if (!file.is_open()) {
      file.open("../../" + path);
      if (!file.is_open()) {
        throw std::runtime_error("Cannot open settings file: " + path);
      }
    }
  }
  file >> config_;
}

ApiConfig Settings::embeddingCurrentApi() const
{
  if (!config_.contains("embedding")) return {};
  return getCurrentApiConfig(config_["embedding"]);
}

std::vector<ApiConfig> Settings::embeddingApis() const
{
  if (!config_.contains("embedding")) return {};
  return getApiConfigList(config_["embedding"]);
}

ApiConfig Settings::generationCurrentApi() const
{
  if (!config_.contains("generation")) return {};
  return getCurrentApiConfig(config_["generation"]);
}

std::vector<ApiConfig> Settings::generationApis() const
{
  if (!config_.contains("generation")) return {};
  return getApiConfigList(config_["generation"]);
}

std::vector<Settings::SourceItem> Settings::sources() const
{
  std::vector<SourceItem> res;
  for (const auto &item : config_["sources"]) {
    SourceItem si;
    si.type = item["type"];
    if (si.type == "directory" || si.type == "file") {
      si.path = item["path"];
    }
    if (si.type == "directory") {
      si.recursive = item.value("recursive", true);
      si.extensions = item.value("extensions", std::vector<std::string>{});
      si.exclude = item.value("exclude", std::vector<std::string>{});
      auto f = filesDefaultExtensions();
      if (si.extensions.empty() && !f.empty()) {
        si.extensions = f;
      }
      auto x = filesGlobalExclusions();
      if (!x.empty()) {
        si.exclude.insert(si.exclude.end(), x.begin(), x.end());
      }
    }
    if (si.type == "url") {
      si.url = item["url"];
      if (item.contains("headers")) {
        for (const auto &[key, value] : item["headers"].items()) {
          std::string headerValue = value;
          // Simple ${VAR} substitution
          if (headerValue.starts_with("${") && headerValue.ends_with("}")) {
            std::string envVar = headerValue.substr(2, headerValue.length() - 3);
            const char *envValue = nullptr;
            envValue = getenv(envVar.c_str());
            if (envValue) {
              headerValue = std::string(envValue);
            }
          }
          si.headers[key] = headerValue;
        }
      }
      si.urlTimeoutMs = item.value("timeout_ms", 10000);
    }
    res.push_back(si);
  }
  return res;
}
