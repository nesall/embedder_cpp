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

  std::vector<Settings::ApiConfig> getApiConfigList(const nlohmann::json &section) {
    std::vector<Settings::ApiConfig> v;
    if (!section.contains("apis") || !section["apis"].is_array()) return v;
    for (const auto &item : section["apis"]) {
      if (!item.is_object()) continue;
      Settings::ApiConfig cfg;
      cfg.id = item.value("id", "");
      cfg.name = item.value("name", "");
      cfg.apiUrl = item.value("api_url", item.value("apiUrl", ""));
      cfg.apiKey = expandEnvVar(item.value("api_key", item.value("apiKey", "")));
      cfg.model = item.value("model", "");
      v.push_back(cfg);
    }
    return v;
  }

  Settings::ApiConfig getCurrentApiConfig(const nlohmann::json &section) {
    Settings::ApiConfig cfg;
    if (!section.is_object()) return cfg;
    std::string current = section.value("current_api", "");
    if (!section.contains("apis") || !section["apis"].is_array()) return cfg;
    for (const auto &item : section["apis"]) {
      if (!item.is_object()) continue;
      std::string id = item.value("id", "");
      if (current.empty() || id == current) {
        cfg.id = id;
        cfg.name = item.value("name", "");
        cfg.apiUrl = item.value("api_url", item.value("apiUrl", ""));
        cfg.apiKey = expandEnvVar(item.value("api_key", item.value("apiKey", "")));
        cfg.model = item.value("model", "");
        break;
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

Settings::ApiConfig Settings::embeddingCurrentApi() const
{
  if (!config_.contains("embedding")) return {};
  return getCurrentApiConfig(config_["embedding"]);
}

std::vector<Settings::ApiConfig> Settings::embeddingApis() const
{
  if (!config_.contains("embedding")) return {};
  return getApiConfigList(config_["embedding"]);
}

Settings::ApiConfig Settings::generationCurrentApi() const
{
  if (!config_.contains("generation")) return {};
  return getCurrentApiConfig(config_["generation"]);
}

std::vector<Settings::ApiConfig> Settings::generationApis() const
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
