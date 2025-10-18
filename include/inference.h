#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>


class App;
struct SearchResult;
struct ApiConfig;


class InferenceClient {
public:
  InferenceClient(const ApiConfig &cfg, size_t timeout);
  virtual ~InferenceClient();

protected:
  struct Impl;
  std::unique_ptr<Impl> imp;

  const ApiConfig &cfg() const;
  std::string path() const;
  std::string schemaHostPort() const;
  size_t timeoutMs() const;
};

class EmbeddingClient : public InferenceClient {
public:
  EmbeddingClient(const ApiConfig &cfg, size_t timeout);
  void generateEmbeddings(const std::vector<std::string> &texts, std::vector<std::vector<float>> &embeddingsList) const;
  void generateEmbeddings(const std::string &text, std::vector<float> &embeddings) const;

  static float calculateL2Norm(const std::vector<float> &vec);
private:
};

class CompletionClient : public InferenceClient {
  const App &app_;
  const size_t maxContextTokens_;
public:
  CompletionClient(const ApiConfig &cfg, size_t timeout, const App &a);
  std::string generateCompletion(
    const nlohmann::json &messages, 
    const std::vector<SearchResult> &searchRes, 
    float temperature,
    size_t maxTokens,
    std::function<void(const std::string &)> onStream) const;

private:

};