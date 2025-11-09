#include "inference.h"
#include "app.h"
#include "database.h"
#include "settings.h"
#include "tokenizer.h"
#include <stdexcept>
#include <cassert>
#include <format>
#include <filesystem>
#include <cmath>  // for std::sqrt
#include <httplib.h>
#include <utils_log/logger.hpp>


struct InferenceClient::Impl {
  ApiConfig apiCfg_;
  std::string schemaHostPort_;
  std::string path_;
  size_t timeoutMs_;

  std::unique_ptr<httplib::Client> httpClient_;

  void parseUrl();
};

void InferenceClient::Impl::parseUrl()
{
  const auto &url = apiCfg_.apiUrl;
  size_t protocolEnd = url.find("://");
  if (protocolEnd == std::string::npos) {
    throw std::runtime_error("Invalid server URL format");
  }
  size_t hostStart = protocolEnd + 3;
  size_t portStart = url.find(":", hostStart);
  size_t pathStart = url.find("/", hostStart);
  schemaHostPort_ = url.substr(0, pathStart);
  path_ = url.substr(pathStart);
}

InferenceClient::InferenceClient(const ApiConfig &cfg, size_t timeout) : imp(new Impl)
{
  imp->apiCfg_ = cfg;
  imp->timeoutMs_ = timeout;
  imp->parseUrl();
  try {
    imp->httpClient_ = std::make_unique<httplib::Client>(schemaHostPort());
    imp->httpClient_->set_connection_timeout(0, timeoutMs() * 1000);
    imp->httpClient_->set_read_timeout(timeoutMs() / 1000, (timeoutMs() % 1000) * 1000);
  } catch (const std::exception &e) {
    LOG_MSG << "Error initializing http client " << e.what();
    imp->httpClient_.reset();
  }
}

InferenceClient::~InferenceClient()
{
}

const ApiConfig &InferenceClient::cfg() const
{
  return imp->apiCfg_;
}

std::string InferenceClient::path() const
{
  return imp->path_;
}

std::string InferenceClient::schemaHostPort() const
{
  return imp->schemaHostPort_;
}

size_t InferenceClient::timeoutMs() const
{
  return imp->timeoutMs_;
}

//---------------------------------------------------------------------------


EmbeddingClient::EmbeddingClient(const ApiConfig &cfg, size_t timeout)
  : InferenceClient(cfg, timeout)
{
}

void EmbeddingClient::generateEmbeddings(const std::vector<std::string> &texts, std::vector<std::vector<float>> &embeddingsList, EmbeddingClient::EncodeType et) const
{
  embeddingsList.reserve(texts.size());
  try {
    if (!imp->httpClient_) {
      throw std::runtime_error("Failed to initialize http client");
    }

    nlohmann::json requestBody;
    requestBody["content"] = prepareContent(texts, et);
    std::string bodyStr = requestBody.dump();

    httplib::Headers headers = {
      {"Content-Type", "application/json"},
      {"Authorization", "Bearer " + cfg().apiKey},
      {"Connection", "keep-alive"}
    };
    auto res = imp->httpClient_->Post(path().c_str(), headers, bodyStr, "application/json");
    if (!res) {
      throw std::runtime_error("Failed to connect to embedding server");
    }
    if (res->status != 200) {
      throw std::runtime_error("Server returned error: " + std::to_string(res->status) + " - " + res->body);
    }
    nlohmann::json response = nlohmann::json::parse(res->body);
    if (!response.is_array() || response.size() != texts.size()) {
      throw std::runtime_error("Unexpected embedding response format");
    }
    for (size_t j = 0; j < texts.size(); j ++) {
      assert(j < response.size());
      if (response.size() <= j) {
        LOG_MSG << "Not enough entries in the embedding response (asked for" << texts.size() << " but got" << response.size() << "). Skipped";
        break;
      }
      const auto &item = response[j];
      if (!item.contains("embedding") || !item["embedding"].is_array()) {
        throw std::runtime_error("Missing or invalid 'embedding' field in response");
      }
      const auto &embeddingArray = item["embedding"];
      if (embeddingArray.empty() || !embeddingArray[0].is_array()) {
        throw std::runtime_error("Invalid embedding structure");
      }
      const auto &embeddingData = embeddingArray[0];
      std::vector<float> embedding;
      embedding.reserve(1024);
      for (const auto &value : embeddingData) {
        if (value.is_number()) {
          embedding.push_back(value.get<float>());
        } else {
          throw std::runtime_error("Non-numeric value in embedding data");
        }
      }
      embeddingsList.push_back(embedding);
    }
    //float l2Norm = calculateL2Norm(embedding);
    //std::cout << "[l2norm] " << l2Norm << std::endl;
  } catch (const nlohmann::json::exception &e) {
    LOG_MSG << "JSON parsing error: " << e.what();
    throw std::runtime_error("Failed to parse server response");
  } catch (const std::exception &e) {
    LOG_MSG << "Error generating embeddings: " << e.what();
    throw;
  }
}

void EmbeddingClient::generateEmbeddings(const std::string &text, std::vector<float> &embeddings, EmbeddingClient::EncodeType et) const
{
  std::vector<std::vector<float>> embs;
  generateEmbeddings({ text }, embs, et);
  if (!embs.empty()) embeddings = std::move(embs.front());
}

float EmbeddingClient::calculateL2Norm(const std::vector<float> &vec)
{
  float sum = 0.0f;
  for (float val : vec) {
    sum += val * val;
  }
  return std::sqrt(sum);
}

std::vector<std::string> EmbeddingClient::prepareContent(const std::vector<std::string> &texts, EmbeddingClient::EncodeType et) const
{
  std::vector<std::string> res{ texts };
  const auto &fmtDoc = imp->apiCfg_.documentFormat;
  const auto &fmtQry = imp->apiCfg_.queryFormat;
  switch (et) {
  case EmbeddingClient::EncodeType::Document:
    if (!fmtDoc.empty() && fmtDoc.find("{}") != std::string::npos) {
      for (auto &t : res) {
        t = std::vformat(fmtDoc, std::make_format_args(t));
      }
    }
    break;
  case EmbeddingClient::EncodeType::Query:
    if (!fmtQry.empty() && fmtQry.find("{}") != std::string::npos) {
      for (auto &t : res) {
        t = std::vformat(fmtQry, std::make_format_args(t));
      }
    }
    break;
  }
  return res;
}


//---------------------------------------------------------------------------


namespace {
  const std::string &_queryTemplate{ R"(
  You're a helpful software developer assistant, please use the provided context to base your answers on
  for user questions. Answer to the best of your knowledge. Keep your responses short and on point.
  Context:
  __CONTEXT__

  Question:
  __QUESTION__

  Answer:
  )" };
} // anonymous namespace

CompletionClient::CompletionClient(const ApiConfig &cfg, size_t timeout, const App &a)
  : InferenceClient(cfg, timeout)
  , app_(a)
{
}

std::string CompletionClient::generateCompletion(
  const nlohmann::json &messagesJson,
  const std::vector<SearchResult> &searchRes,
  float temperature,
  size_t maxTokens,
  std::function<void(const std::string &)> onStream) const
{
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
  if (schemaHostPort().starts_with("https://")) {
    throw std::runtime_error("HTTPS not supported in this build");
  }
#endif
  if (!imp->httpClient_) {
    throw std::runtime_error("Failed to initialize http client");
  }

  /*
  * Json Request body format.
  {
    "model": "",
    "messages": [
      {"role": "system", "content": "Keep it short."},
      {"role": "user", "content": "What is the capital of France?"}
    ],
    "temperature": 0.7
   }
  */

  if (onStream) {
    onStream("[meta]Working on the response");
  }


  const auto labelFmt = app_.settings().generationPrependLabelFormat();
  const auto maxContextTokens = cfg().contextLength;
  size_t nofTokens = app_.tokenizer().countTokensWithVocab(_queryTemplate);
  std::string context;
  for (const auto &r : searchRes) {
    std::string filename = std::filesystem::path(r.sourceId).filename().string();
    if (filename.empty()) filename = r.sourceId.empty() ? "source" : r.sourceId;
    // format label (std::vformat requires C++20)
    std::string label = std::vformat(labelFmt, std::make_format_args(filename));
    // avoid double-labeling
    bool alreadyLabeled = (r.content.rfind(label, 0) == 0);

    size_t contentTokens = app_.tokenizer().countTokensWithVocab(r.content);
    size_t labelTokens = alreadyLabeled ? 0 : app_.tokenizer().countTokensWithVocab(label);

    if (maxContextTokens < nofTokens + + labelTokens + contentTokens) {
      size_t remaining = (nofTokens < maxContextTokens) ? (maxContextTokens - nofTokens) : 0;
      if (remaining <= labelTokens) {
        // can't fit label + content excerpt -> stop
        break;
      }
      size_t remainingContentTokens = remaining - labelTokens;
      if (remainingContentTokens == 0) break;
      
      // approximate characters for remainingContentTokens
      size_t approxCharCount = r.content.length();
      if (0 < contentTokens) {
        approxCharCount = r.content.length() * remainingContentTokens / contentTokens;
      }

      std::string excerpt = r.content.substr(0, approxCharCount);

      std::string labeledExcerpt = alreadyLabeled ? excerpt : (label + excerpt);
      context += labeledExcerpt + "\n\n";
      nofTokens += app_.tokenizer().countTokensWithVocab(labeledExcerpt);
      break;
    }
    // full add
    std::string labeledFull = alreadyLabeled ? r.content : (label + r.content);
    nofTokens += labelTokens + contentTokens;
    context += labeledFull + "\n\n";
  }

  //std::cout << "Generating completions with context length of " << nofTokens << " tokens \n";

  std::string prompt = _queryTemplate;
  size_t pos = prompt.find("__CONTEXT__");
  assert(pos != std::string::npos);
  prompt.replace(pos, std::string("__CONTEXT__").length(), context);

  pos = prompt.find("__QUESTION__");
  assert(pos != std::string::npos);
  std::string question = messagesJson.back()["content"].get<std::string>();
  prompt.replace(pos, std::string("__QUESTION__").length(), question);

  // Assign propmt to the last messagesJson's content field
  nlohmann::json modifiedMessages = messagesJson;
  modifiedMessages.back()["content"] = prompt;

  //std::cout << "Full context: " << modifiedMessages.dump() << "\n";

  nlohmann::json requestBody;
  requestBody["model"] = cfg().model;
  requestBody["messages"] = modifiedMessages;
  if (cfg().temperatureSupport)
    requestBody["temperature"] = temperature;
  requestBody[cfg().maxTokensName] = maxTokens;
  requestBody["stream"] = cfg().stream;

  httplib::Headers headers = {
    {"Authorization", "Bearer " + cfg().apiKey},
    {"Connection", "keep-alive"}
  };

  std::string fullResponse;
  httplib::Result res;

  if (cfg().stream) {
    headers.insert({ "Accept", "text/event-stream" });

    std::string buffer; // holds leftover partial data

    res = imp->httpClient_->Post(
      path().c_str(),
      headers,
      requestBody.dump(),
      "application/json",
      [&fullResponse, &onStream, &buffer](const char *data, size_t len) {
        // llama-server sends SSE format: "data: {...}\n\n"
        buffer.append(data, len);
        size_t pos;
        while ((pos = buffer.find("\n\n")) != std::string::npos) {
          std::string event = buffer.substr(0, pos); // one SSE event
          buffer.erase(0, pos + 2);
          if (event.find("data: ", 0) == 0) {
            std::string jsonStr = event.substr(6);
            if (jsonStr == "[DONE]") {
              break;
            }
            try {
              nlohmann::json chunkJson = nlohmann::json::parse(jsonStr);
              if (chunkJson.contains("choices") && !chunkJson["choices"].empty()) {
                const auto &choice = chunkJson["choices"][0];
                if (choice.contains("delta") && choice["delta"].contains("content")) {
                  // Either choice["delta"]["content"] or choice["delta"]["reasoning_content"]
                  std::string content;
                  if (!choice["delta"]["content"].is_null())
                    content = choice["delta"]["content"];
                  else if (choice["delta"].contains("reasoning_content") && !choice["delta"]["reasoning_content"].is_null())
                    content = choice["delta"]["reasoning_content"];
                  fullResponse += content;
                  if (onStream) {
                    onStream(content);
                  }
                }
              }
            } catch (const std::exception &e) {
              LOG_MSG << "Error parsing chunk: " << e.what() << " in: " << jsonStr;
            }
          }
        }
        if (buffer.find("Unauthorized") != std::string::npos) {
          if (onStream) onStream(buffer);
        }
        return true; // Continue receiving
      }
    );

  } else {
    headers.insert({ "Accept", "application/json" });

    res = imp->httpClient_->Post(
      path().c_str(),
      headers,
      requestBody.dump(),
      "application/json"
    );

    if (res && res->status == 200) {
      try {
        nlohmann::json jsonRes = nlohmann::json::parse(res->body);
        if (!jsonRes["choices"].empty()) {
          const auto &choice = jsonRes["choices"][0];
          if (choice.contains("message") && choice["message"].contains("content")) {
            fullResponse = choice["message"]["content"];
            if (onStream) onStream(fullResponse); // optional callback for consistency
          }
        }
      } catch (...) { /* ignore parse errors */ }
    }
  }

  if (!res) {
    throw std::runtime_error("Failed to connect to completion server");
  }

  if (res->status != 200) {
    std::string msg = std::format("Server returned error: {} - {}", res->status, res->body);
    if (onStream) onStream(msg);
    throw std::runtime_error(msg);
  }

  return fullResponse;
}
