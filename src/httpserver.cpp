#include "httpserver.h"
#include "app.h"
#include "chunker.h"
#include "sourceproc.h"
#include "database.h"
#include "inference.h"
#include "settings.h"
#include "tokenizer.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <utils_log/logger.hpp>
#include <cassert>
#include <thread>
#include <exception>
#include <algorithm>
#include <set>

using json = nlohmann::json;


namespace {

  //auto testStreaming = [](std::function<void(const std::string &)> onChunk)
  //  {
  //    for (int i = 0; i < 25; ++i) {
  //      std::this_thread::sleep_for(std::chrono::milliseconds(250));
  //      std::ostringstream oss;
  //      oss << "thread ";
  //      oss << std::this_thread::get_id();
  //      oss << ", chunk ";
  //      oss << std::to_string(i);
  //      oss << "\n\n";
  //      onChunk(oss.str());
  //    }
  //  };

  template <class T>
  bool vecContains(const std::vector<T> &vec, const T &t) {
    return std::find(vec.begin(), vec.end(), t) != vec.end();
  }

  template <class T>
  void vecAddIfUnique(std::vector<T> &vec, const T &t) {
    if (!vecContains(vec, t)) {
      vec.push_back(t);
    }
  }

  template <class T>
  void vecAddIfUnique(std::vector<T> &vec, const std::vector<T> &t) {
    for (const auto &item : t) {
      vecAddIfUnique(vec, item);
    }
  }

  struct Attachment {
    std::string filename;
    std::string content;
  };

  std::vector<Attachment> parseAttachments(const json &attachmentsJson) {
    std::vector<Attachment> res;
    if (!attachmentsJson.is_array()) return res;
    for (const auto &item : attachmentsJson) {
      if (!item.is_object()) continue;
      if (!item.contains("content") || !item["content"].is_string()) {
        continue;
      }
      Attachment a;
      if (item.contains("filename") && item["filename"].is_string()) {
        a.filename = item["filename"].get<std::string>();
      }
      a.content = item["content"].get<std::string>();
      if (!a.filename.empty()) {
        a.content = "[Attachment: " + a.filename + "]\n" + a.content + "\n[/Attachment]";
      }
      res.push_back(std::move(a));
    }
    return res;
  }

} // anonymous namespace


struct HttpServer::Impl {
  Impl(App &a)
    : app_(a)
  {
  }

  httplib::Server server_;

  App &app_;

  //std::unique_ptr<std::thread> watchThread_;
  //std::atomic<bool> watchRunning_{ false };

  static int counter_;
};

int HttpServer::Impl::counter_ = 0;

HttpServer::HttpServer(App &a)
  : imp(new Impl(a))
{
  //imp->server_.new_task_queue = [] { return new httplib::ThreadPool(4); };
}

HttpServer::~HttpServer()
{
}

bool HttpServer::startServer(int port)
{
  auto &server = imp->server_;

  server.Get("/api/health", [](const httplib::Request &, httplib::Response &res) {
    LOG_MSG << "GET /api/health";
    json response = { {"status", "ok"} };
    res.set_content(response.dump(), "application/json");
    });

  server.Post("/api/search", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      LOG_MSG << "POST /api/search";
      json request = json::parse(req.body);

      std::string query = request["query"].get<std::string>();
      size_t top_k = request.value("top_k", 5);
      std::vector<float> queryEmbedding;
      EmbeddingClient embeddingClient(imp->app_.settings().embeddingCurrentApi(), imp->app_.settings().embeddingTimeoutMs());
      embeddingClient.generateEmbeddings(query, queryEmbedding, EmbeddingClient::EncodeType::Query);

      auto results = imp->app_.db().search(queryEmbedding, top_k);

      json response = json::array();
      for (const auto &result : results) {
        response.push_back({
            {"content", result.content},
            {"source_id", result.sourceId},
            {"chunk_type", result.chunkType},
            {"chunk_unit", result.chunkUnit},
            {"similarity_score", result.similarityScore},
            {"start_pos", result.start},
            {"end_pos", result.end}
          });
      }

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  // (one-off embedding without storage)
  server.Post("/api/embed", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      LOG_MSG << "POST /api/embed";
      json request = json::parse(req.body);
      std::string text = request["text"].get<std::string>();
      auto chunks = imp->app_.chunker().chunkText(text, "api-request");
      std::vector<std::string> texts;
      for (const auto &c : chunks) {
        texts.push_back(c.text);
      }
      json response = json::array();
      const auto &ss = imp->app_.settings();
      const auto batchSize = ss.embeddingBatchSize();
      EmbeddingClient embeddingClient(ss.embeddingCurrentApi(), ss.embeddingTimeoutMs());
      for (size_t i = 0; i < chunks.size(); i += batchSize) {
        size_t end = (std::min)(i + batchSize, chunks.size());
        std::vector<std::string> batchTexts(texts.begin() + i, texts.begin() + end);

        std::vector<std::vector<float>> embeddings;
        embeddingClient.generateEmbeddings(batchTexts, embeddings, EmbeddingClient::EncodeType::Query);

        for (const auto &emb : embeddings) {
          response.push_back({ {"embedding", emb}, {"dimension", emb.size()} });
        }
      }

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Post("/api/documents", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      LOG_MSG << "POST /api/documents";
      json request = json::parse(req.body);

      std::string content = request["content"].get<std::string>();
      std::string source_id = request["source_id"].get<std::string>();

      auto chunks = imp->app_.chunker().chunkText(content, source_id);

      EmbeddingClient embeddingClient(imp->app_.settings().embeddingCurrentApi(), imp->app_.settings().embeddingTimeoutMs());
      size_t inserted = 0;
      for (const auto &chunk : chunks) {
        std::vector<float> embedding;
        embeddingClient.generateEmbeddings(chunk.text, embedding, EmbeddingClient::EncodeType::Index);
        imp->app_.db().addDocument(chunk, embedding);
        inserted++;
      }

      imp->app_.db().persist();

      json response = {
          {"status", "success"},
          {"chunks_added", inserted}
      };

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Get("/api/documents", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      LOG_MSG << "GET /api/documents";
      auto files = imp->app_.db().getTrackedFiles();
      json response = json::array();
      for (const auto &file : files) {
        response.push_back({
            {"path", file.path},
            {"lastModified", file.lastModified},
            {"size", file.fileSize}
          });
      }
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Get("/api/stats", [this](const httplib::Request &, httplib::Response &res) {
    try {
      LOG_MSG << "GET /api/stats";
      auto stats = imp->app_.db().getStats();
      json sources_obj = json::object();
      for (const auto &[source, count] : stats.sources) {
        sources_obj[source] = count;
      }
      json response = {
          {"total_chunks", stats.totalChunks},
          {"vector_count", stats.vectorCount},
          {"sources", sources_obj}
      };
      res.set_content(response.dump(), "application/json");
      LOG_MSG << std::this_thread::get_id() << " Starting /stats ...";
      std::this_thread::sleep_for(std::chrono::seconds(20));
      LOG_MSG << std::this_thread::get_id() << " Finished /stats !";
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Post("/api/update", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      LOG_MSG << "POST /api/update";
      auto nof = imp->app_.update();
      json response = { {"status", "updated"}, {"nof_files", std::to_string(nof)} };
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Post("/api/chat", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      LOG_MSG << "POST /api/chat";
      // format for messages field in request
      /*
      {
        "messages": [
          {"role": "system", "content": "Keep it short."},
          {"role": "user", "content": "What is the capital of France?"}
        ],
        "sourceids": [
          "../embedder_cpp/src/main.cpp", "../embedder_cpp/include/settings.h"
        ],
        "attachments": [
          { "filename": "filename1.cpp", "content": "..text file content 1.."},
          { "filename": "filename2.cpp", "content": "..text file content 2.."},
        ],
        "temperature": 0.2,
        "max_tokens": 800,
        "targetapi": "xai"
      }  
      */
      json request = json::parse(req.body);
      if (!request.contains("messages") || !request["messages"].is_array() || request["messages"].empty()) {
        throw std::invalid_argument("'messages' field required and must be non-empty array");
      }
      const auto messagesJson = request["messages"];
      if (0 == messagesJson.size()) {
        throw std::invalid_argument("'messages' array must be non-empty");
      }
      if (!messagesJson.back().contains("role") || !messagesJson.back().contains("content")) {
        throw std::invalid_argument("Last message must have 'role' and 'content' fields");
      }

      std::string role = messagesJson.back()["role"];
      if (role != "user") {
        throw std::invalid_argument("Last message role must be 'user', got: " + role);
      }
      const float temperature = request.value("temperature", imp->app_.settings().generationDefaultTemperature());
      const size_t maxTokens = request.value("max_tokens", imp->app_.settings().generationDefaultMaxTokens());
      std::string question = messagesJson.back()["content"].get<std::string>();

      auto attachmentsJson = request["attachments"];
      auto attachments = parseAttachments(attachmentsJson);

      // Preferred order
      std::vector<SearchResult> attachmentResults;
      std::vector<SearchResult> fullSourceResults;
      std::vector<SearchResult> relatedSrcResults;
      std::vector<SearchResult> filteredChunkResults;
      std::vector<SearchResult> orderedResults; // Final ordered results

      for (const auto &att : attachments) {
        attachmentResults.push_back({
          att.content,
          att.filename.empty() ? "attachment" : att.filename,
          "char",
          Chunker::contentTypeToStr(Chunker::detectContentType(att.content, "")),
          std::string::npos, // chunkId
          0,
          att.content.size(),
          1.0f
          });
      }

      std::vector<std::string> sources;
      if (request.contains("sourceids")) {
        auto sourceidsJson = request["sourceids"];
        for (const auto &sid : sourceidsJson) {
          if (sid.is_string()) {
            vecAddIfUnique(sources, sid.get<std::string>());
          }
        }
      }

      EmbeddingClient embeddingClient(imp->app_.settings().embeddingCurrentApi(), imp->app_.settings().embeddingTimeoutMs());
      std::unordered_map<std::string, float> sourcesRank;
      const auto questionChunks = imp->app_.chunker().chunkText(question, "", false);
      for (const auto &qc : questionChunks) {
        std::vector<float> embedding;
        embeddingClient.generateEmbeddings(qc.text, embedding, EmbeddingClient::EncodeType::Query);
        auto res = imp->app_.db().search(embedding, imp->app_.settings().embeddingTopK());
        filteredChunkResults.insert(filteredChunkResults.end(), res.begin(), res.end());
        for (const auto &r : res) {
          sourcesRank[r.sourceId] += r.similarityScore;
        }
      }

      std::sort(filteredChunkResults.begin(), filteredChunkResults.end(), [&sourcesRank](const SearchResult &a, const SearchResult &b) {
        return sourcesRank[a.sourceId] > sourcesRank[b.sourceId];
        });

      const auto maxFullSources = imp->app_.settings().generationMaxFullSources();
      for (const auto r : filteredChunkResults) {
        vecAddIfUnique(sources, r.sourceId);
        if (sources.size() == maxFullSources) break;
      }

      const auto trackedFiles = imp->app_.db().getTrackedFiles();
      std::vector<std::string> trackedSources;
      for (const auto &tf : trackedFiles) {
        trackedSources.push_back(tf.path);
      }

      std::vector<std::string> allFullSources{ sources };
      std::vector<std::string> relSources;
      for (const auto &src : sources) {
        auto relations = imp->app_.sourceProcessor().filterRelatedSources(trackedSources, src);
        if (!vecContains(allFullSources, src)) {
          vecAddIfUnique(relSources, relations);
          vecAddIfUnique(allFullSources, relations);
        }
      }

      for (const auto &src : sources) {
        auto data = imp->app_.sourceProcessor().fetchSource(src);
        if (!data.content.empty()) {
          fullSourceResults.push_back({
              data.content,
              src,
              "char",
              Chunker::contentTypeToStr(Chunker::detectContentType(data.content, "")),
              std::string::npos, // chunkId
              0,
              data.content.length(),
              1.0f
            });
        }
      }
      for (const auto &rel : relSources) {
        auto data = imp->app_.sourceProcessor().fetchSource(rel);
        if (!data.content.empty()) {
          relatedSrcResults.push_back({
              data.content,
              rel,
              "char",
              Chunker::contentTypeToStr(Chunker::detectContentType(data.content, "")),
              std::string::npos,
              0,
              data.content.length(),
              1.0f,
            });
        }
      }

      filteredChunkResults.erase(std::remove_if(filteredChunkResults.begin(), filteredChunkResults.end(),
        [&allFullSources](const SearchResult &r) {
          //return allFullSources.find(r.sourceId) != allFullSources.end() && r.chunkId != std::string::npos;
          return vecContains(allFullSources, r.sourceId) && r.chunkId != std::string::npos;
        }), filteredChunkResults.end());

      // Assemble final ordered results
      orderedResults.insert(orderedResults.end(), attachmentResults.begin(), attachmentResults.end());
      orderedResults.insert(orderedResults.end(), fullSourceResults.begin(), fullSourceResults.end());
      orderedResults.insert(orderedResults.end(), relatedSrcResults.begin(), relatedSrcResults.end());
      orderedResults.insert(orderedResults.end(), filteredChunkResults.begin(), filteredChunkResults.end());
      if (imp->app_.settings().generationMaxChunks() < orderedResults.size()) {
        orderedResults.resize(imp->app_.settings().generationMaxChunks());
      }

      ApiConfig apiConfig = imp->app_.settings().generationCurrentApi();
      if (request.contains("targetapi")) {
        std::string targetApi = request["targetapi"];
        auto apis = imp->app_.settings().generationApis();
        auto it = std::find_if(apis.begin(), apis.end(), [&targetApi](const ApiConfig &a) { return a.id == targetApi; });
        if (it != apis.end()) {
          apiConfig = *it;
        }
      }

      res.set_header("Content-Type", "text/event-stream");
      res.set_header("Cache-Control", "no-cache");
      res.set_header("Connection", "keep-alive");

      res.set_chunked_content_provider(
        "text/event-stream",
        [this, messagesJson, orderedResults, temperature, maxTokens, apiConfig](size_t offset, httplib::DataSink &sink) {
          //LOG_MSG << "set_chunked_content_provider: in callback ...";
          CompletionClient completionClient(apiConfig, imp->app_.settings().generationTimeoutMs(), imp->app_);
          try {
            std::string context = completionClient.generateCompletion(
              messagesJson, orderedResults, temperature, maxTokens,
              [&sink](const std::string &chunk) {
#ifdef _DEBUG2
                LOG_MSG << chunk;
#endif
                // SSE format requires "data: <payload>\n\n"
                nlohmann::json payload = { {"content", chunk} };
                std::string sse = "data: " + payload.dump() + "\n\n";
                bool success = sink.write(sse.data(), sse.size());
                if (!success) {
                  return; // Client disconnected
                }
              });

#ifdef _DEBUG2
            testStreaming([&sink](const std::string &chunk) {
              if (!sink.write(chunk.data(), chunk.size())) {
                return; // client disconnected
              }
              });
#endif

            // Add sources information
            nlohmann::json sourcesJson;
            //std::set<std::string> sources;
            for (const auto &result : orderedResults) {
              //sources.insert(result.sourceId);
              sourcesJson.push_back(result.sourceId);
            }

            //for (const auto &source : sources) {
            //}

            // Send sources information as a separate SSE message
            nlohmann::json sourcesPayload = {
              {"sources", sourcesJson},
              {"type", "context_sources"}
            };
            std::string sourcesSse = "data: " + sourcesPayload.dump() + "\n\n";
            sink.write(sourcesSse.data(), sourcesSse.size());


            std::string done = "data: [DONE]\n\n";
            sink.write(done.data(), done.size());
            sink.done();
          } catch (const std::exception &e) {
            std::string error = "data: {\"error\": \"" + std::string(e.what()) + "\"}\n\n";
            sink.write(error.data(), error.size());
            sink.done();
          }
          //LOG_MSG << "set_chunked_content_provider: callback DONE.";
          return true;
        }
      );

      } catch (const std::exception &e) {
        json error = { {"error", e.what()} };
        res.status = 400;
        res.set_content(error.dump(), "application/json");
      }
    });

    server.Get("/api/settings", [this](const httplib::Request &, httplib::Response &res) {
      try {
        LOG_MSG << "GET /api/settings";
        nlohmann::json apisJson;
        const auto &cur = imp->app_.settings().generationCurrentApi();
        const auto &apis = imp->app_.settings().generationApis();
        for (const auto &api : apis) {
          nlohmann::json apiObj;
          apiObj["id"] = api.id;
          apiObj["name"] = api.name;
          apiObj["url"] = api.apiUrl;
          apiObj["model"] = api.model;
          apiObj["current"] = (api.id == cur.id);
          apiObj["combinedPrice"] = api.combinedPrice();
          apisJson.push_back(apiObj);
        }
        nlohmann::json responseJson;
        responseJson["completionApis"] = apisJson;
        responseJson["currentApi"] = cur.id;
        res.status = 200;
        res.set_content(responseJson.dump(2), "application/json");
      } catch (const std::exception &e) {
        nlohmann::json errorJson;
        errorJson["error"] = "Failed to load settings";
        errorJson["message"] = e.what();
        res.status = 500;
        res.set_content(errorJson.dump(2), "application/json");
      }
      });

    server.Get("/api", [](const httplib::Request &, httplib::Response &res) {
      LOG_MSG << "GET /api";
      json info = {
          {"name", "Embeddings RAG API"},
          {"version", "1.0.0"},
          {"endpoints", {
              {"GET /api/health", "Health check"},
              {"GET /api/documents", "Get documents"},
              {"GET /api/stats", "Database statistics"},
              {"GET /api/settings", "Available APIs"},
              {"POST /api/search", "Semantic search"},
              {"POST /api/chat", "Chat with context (streaming)"},
              {"POST /api/embed", "Generate embeddings"},
              {"POST /api/documents", "Add documents"},
              {"POST /api/update", "Trigger manual update"},
          }}
      };
      res.set_content(info.dump(2), "application/json");
      });

    LOG_MSG << "Starting HTTP API server on port " << port << "...";

    //if (enableWatch) {
    //  startWatch(watchInterval);
    //  LOG_MSG << "  Auto-update: enabled (every " << watchInterval << "s)";
    //} else {
    //  LOG_MSG << "  Auto-update: disabled";
    //}

    LOG_MSG << "\nEndpoints:";
    LOG_MSG << "  GET  /api";
    LOG_MSG << "  GET  /api/health";
    LOG_MSG << "  GET  /api/stats";
    LOG_MSG << "  GET  /api/settings";
    LOG_MSG << "  GET  /api/documents";
    LOG_MSG << "  POST /api/search    - {\"query\": \"...\", \"top_k\": 5}";
    LOG_MSG << "  POST /api/embed     - {\"text\": \"...\"}";
    LOG_MSG << "  POST /api/documents - {\"content\": \"...\", \"source_id\": \"...\"}";
    LOG_MSG << "  POST /api/chat      - {\"messages\":[\"role\":\"...\", \"content\":\"...\"], \"temperature\": \"...\"}";
    LOG_MSG << "\nPress Ctrl+C to stop";

    return server.listen("0.0.0.0", port);
}

void HttpServer::stop()
{
  if (imp->server_.is_running()) {
    LOG_MSG << "Server stopping...";
    imp->server_.stop();
    LOG_MSG << "Server stopped!";
  }
}
//
//void HttpServer::startWatch(int intervalSeconds)
//{
//  imp->watchRunning_ = true;
//  imp->watchThread_ = std::make_unique<std::thread>([this, intervalSeconds]() {
//    LOG_MSG << "[Watch] Background monitoring started (interval: " << intervalSeconds << "s)";
//    while (imp->watchRunning_) {
//      // Sleep in small chunks so we can stop quickly
//      for (int i = 0; i < intervalSeconds && imp->watchRunning_; ++i) {
//        std::this_thread::sleep_for(std::chrono::seconds(1));
//      }
//      if (!imp->watchRunning_) break;
//      try {
//        imp->app_.update();
//      } catch (const std::exception &e) {
//        std::cerr << "[Watch] Error during update: " << e.what();
//      }
//    }
//    LOG_MSG << "[Watch] Background monitoring stopped";
//    });
//}
//
//void HttpServer::stopWatch()
//{
//  if (imp->watchRunning_) {
//    LOG_MSG << "Stopping watch mode...";
//    imp->watchRunning_ = false;
//    if (imp->watchThread_ && imp->watchThread_->joinable()) {
//      imp->watchThread_->join();
//    }
//  }
//}