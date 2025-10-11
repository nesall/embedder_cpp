#include "inference.h"
#include "app.h"
#include "database.h"
#include "settings.h"
#include "tokenizer.h"
#include <stdexcept>
#include <iostream>
#include <cmath>  // for std::sqrt
#include <httplib.h>

InferenceClient::InferenceClient(const Settings::ApiConfig &cfg, size_t timeout)
  : apiCfg_(cfg)
  , timeoutMs_(timeout)
{
  parseUrl();
}

void InferenceClient::parseUrl()
{
  const auto &url = apiCfg_.apiUrl;
  size_t protocolEnd = url.find("://");
  if (protocolEnd == std::string::npos) {
    throw std::runtime_error("Invalid server URL format");
  }
  size_t hostStart = protocolEnd + 3;
  size_t port_start = url.find(":", hostStart);
  size_t path_start = url.find("/", hostStart);
  if (port_start != std::string::npos && port_start < path_start) {
    host_ = url.substr(hostStart, port_start - hostStart);
    port_ = std::stoi(url.substr(port_start + 1, path_start - port_start - 1));
  } else {
    host_ = url.substr(hostStart, path_start - hostStart);
    port_ = url.starts_with("https:") ? 443 : 80;
  }

  path_ = url.substr(path_start);
}


//---------------------------------------------------------------------------


EmbeddingClient::EmbeddingClient(const Settings &ss)
  : InferenceClient(ss.embeddingCurrentApi(), ss.embeddingTimeoutMs())
{
}

void EmbeddingClient::generateEmbeddings(const std::vector<std::string> &texts, std::vector<float> &embedding) const
{
  embedding.reserve(1024);
  try {
    httplib::Client client(host_, port_);
    client.set_connection_timeout(0, timeoutMs_ * 1000);
    client.set_read_timeout(timeoutMs_ / 1000, (timeoutMs_ % 1000) * 1000);

    nlohmann::json requestBody;
    requestBody["content"] = texts;
    std::string bodyStr = requestBody.dump();

    httplib::Headers headers = {
      {"Content-Type", "application/json"},
      {"Authorization", "Bearer " + apiCfg_.apiKey}
    };
    auto res = client.Post(path_.c_str(), headers, bodyStr, "application/json");
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
    const auto &item = response[0];
    if (!item.contains("embedding") || !item["embedding"].is_array()) {
      throw std::runtime_error("Missing or invalid 'embedding' field in response");
    }
    const auto &embeddingArray = item["embedding"];
    if (embeddingArray.empty() || !embeddingArray[0].is_array()) {
      throw std::runtime_error("Invalid embedding structure");
    }
    const auto &embeddingData = embeddingArray[0];
    for (const auto &value : embeddingData) {
      if (value.is_number()) {
        embedding.push_back(value.get<float>());
      } else {
        throw std::runtime_error("Non-numeric value in embedding data");
      }
    }
    //float l2Norm = calculateL2Norm(embedding);
    //std::cout << "[l2norm] " << l2Norm << std::endl;
  } catch (const nlohmann::json::exception &e) {
    std::cerr << "JSON parsing error: " << e.what() << std::endl;
    throw std::runtime_error("Failed to parse server response");
  } catch (const std::exception &e) {
    std::cerr << "Error generating embeddings: " << e.what() << std::endl;
    throw;
  }
}

std::vector<std::vector<float>> EmbeddingClient::generateBatchEmbeddings(const std::vector<std::string> &texts) const
{
  std::vector<std::vector<float>> results;
  for (const auto &text : texts) {
    std::vector<float> embedding;
    generateEmbeddings({ text }, embedding);
    results.push_back(embedding);
  }
  return results;
}

float EmbeddingClient::calculateL2Norm(const std::vector<float> &vec)
{
  float sum = 0.0f;
  for (float val : vec) {
    sum += val * val;
  }
  return std::sqrt(sum);
}


//---------------------------------------------------------------------------


namespace {
  const std::string &_queryTemplate{ R"(
  You're a helpful software developer assistent, please use the provided context to base your answers on
  for user questions. Answer to the best of your knowledge. Keep your responses short and on point.
  Context:
  __CONTEXT__

  Question:
  __QUESTION__
  )" };
} // anonymous namespace

CompletionClient::CompletionClient(const App &a)
  : InferenceClient(a.settings().generationCurrentApi(), a.settings().generationTimeoutMs())
  , app_(a)
  , maxContextTokens_(a.settings().generationMaxContextTokens())
{
}

std::string CompletionClient::generateCompletion(
  const nlohmann::json &messagesJson, 
  const std::vector<SearchResult> &searchRes, 
  float temperature,
  std::function<void(const std::string &)> onStream) const
{
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  httplib::SSLClient client(host_, port_);  
#else 
  if (apiCfg_.apiUrl.starts_with("https://")) {
    throw std::runtime_error("HTTPS not supported in this build");
  }
  httplib::Client client(host_, port_);
#endif
  client.set_connection_timeout(0, timeoutMs_ * 1000);
  client.set_read_timeout(timeoutMs_ / 1000, (timeoutMs_ % 1000) * 1000);

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

  size_t nofTokens = app_.tokenizer().countTokensWithVocab(_queryTemplate);

  std::string question = messagesJson.back()["content"].get<std::string>();

  std::string context;
  for (const auto &r : searchRes) {
    auto nt = app_.tokenizer().countTokensWithVocab(r.content);
    if (maxContextTokens_ < nofTokens + nt) {
      size_t remaining = maxContextTokens_ - nofTokens;
      if (0 < remaining) {
        size_t approxCharCount = r.content.length() * remaining / nt;
        context += r.content.substr(0, approxCharCount) + "\n\n";
        nofTokens += app_.tokenizer().countTokensWithVocab(r.content.substr(0, approxCharCount));
      }
      break;
    }
    nofTokens += nt;
    context += r.content + "\n\n";
  }

  //std::cout << "Generating completions with context length of " << nofTokens << " tokens \n";

  std::string prompt = _queryTemplate;
  size_t pos = prompt.find("__CONTEXT__");
  assert(pos != std::string::npos);
  prompt.replace(pos, std::string("__CONTEXT__").length(), context);
  
  pos = prompt.find("__QUESTION__");
  assert(pos != std::string::npos);
  prompt.replace(pos, std::string("__QUESTION__").length(), question);  

  // Assign propmt to the last messagesJson's content field
  nlohmann::json modifiedMessages = messagesJson;
  modifiedMessages.back()["content"] = prompt;

  //std::cout << "Full context: " << modifiedMessages.dump() << "\n";

  nlohmann::json requestBody;
  requestBody["model"] = apiCfg_.model;
  requestBody["messages"] = modifiedMessages;
  requestBody["temperature"] = temperature;
  requestBody["stream"] = true;

  httplib::Headers headers = {
    {"Accept", "text/event-stream"},
    {"Authorization", "Bearer " + apiCfg_.apiKey}
  };

  std::string fullResponse;
  std::string buffer; // holds leftover partial data

  auto res = client.Post(
    path_.c_str(),
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
                else if (!choice["delta"]["reasoning_content"].is_null())
                  content = choice["delta"]["reasoning_content"];
                fullResponse += content;
                if (onStream) {
                  onStream(content);
                }
              }
            }
          } catch (const std::exception &e) {
            std::cerr << "Error parsing chunk: " << e.what() << " in: " << jsonStr << std::endl;
          }
        }
      }
      return true; // Continue receiving
    }
  );

  if (!res) {
    throw std::runtime_error("Failed to connect to completion server");
  }

  if (res->status != 200) {
    throw std::runtime_error("Server returned error: " + std::to_string(res->status) + " " + res->reason + " - " + res->body);
  }

  return fullResponse;
}
