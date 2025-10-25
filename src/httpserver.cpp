#include "httpserver.h"
#include "app.h"
#include "chunker.h"
#include "sourceproc.h"
#include "database.h"
#include "inference.h"
#include "settings.h"
#include "tokenizer.h"
#include "auth.h"
#include "3rdparty/base64.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <utils_log/logger.hpp>
#include <chrono>
#include <cassert>
#include <exception>
#include <algorithm>
#include <atomic>

using json = nlohmann::json;


namespace {

#if 0
  auto testStreaming = [](std::function<void(const std::string &)> onChunk)
    {
      for (int i = 0; i < 25; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::ostringstream oss;
        oss << "thread ";
        oss << std::this_thread::get_id();
        oss << ", chunk ";
        oss << std::to_string(i);
        oss << "\n\n";
        onChunk(oss.str());
      }
    };
#endif

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

  void addToSearchResult(std::vector<SearchResult> &v, const std::string &src, const std::string &content) {
    assert(!content.empty());
    if (!content.empty()) {
      v.push_back({
          content,
          src,
          "char",
          Chunker::contentTypeToStr(Chunker::detectContentType(content, "")),
          std::string::npos,
          0,
          content.length(),
          1.0f
        });
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

  void recordDuration(const std::chrono::steady_clock::time_point &start, std::atomic<double> &v) {
    std::chrono::duration<double> duration = std::chrono::steady_clock::now() - start;
    double newSearchTimeMs = duration.count() * 1000.0;
    double oldVal = v.load();
    double newVal;
    do {
      newVal = oldVal * 0.9 + newSearchTimeMs * 0.1;
    } while (!v.compare_exchange_weak(oldVal, newVal));
  }

  std::optional<std::pair<std::string, std::string>> extractPassword(const httplib::Request &req) {
    try {
      auto header = req.get_header_value("Authorization");
      if (header.find("Basic ") == 0) {
        std::string encoded = header.substr(6);
        std::string decoded = base64_decode(encoded);
        // Format is "username:password" - we only care about password
        size_t colon = decoded.find(':');
        if (colon == std::string::npos) {
          return std::nullopt;
        }
        return std::pair{ decoded.substr(colon + 1), "Basic" };
      } else if (header.find("Bearer ") == 0) {
        std::string encoded = header.substr(7);
        return std::pair{ encoded, "Bearer" };
      }
    } catch (const std::exception &e) {
      LOG_MSG << "Error extracting password" << e.what();
    }
    return std::nullopt;
  }

  bool requireAuth(AdminAuth &auth, const httplib::Request &req, httplib::Response &res, std::string *jwtToken) {
    auto password = extractPassword(req);
    std::string jwt;
    if (!password.has_value() || !auth.authenticate(password.value(), jwt)) {
      res.status = 401;
      res.set_header("WWW-Authenticate", "Basic realm=\"Embedder Admin\"");
      res.set_content(R"({"error": "Authentication required"})", "application/json");
      return false;
    }
    if (jwtToken) *jwtToken = jwt;
    return true;
  }
} // anonymous namespace


struct HttpServer::Impl {
  Impl(App &a)
    : app_(a)
  {
  }

  httplib::Server server_;

  App &app_;

  static std::atomic<size_t> requestCounter_;
  static std::atomic<size_t> searchCounter_;
  static std::atomic<size_t> chatCounter_;
  static std::atomic<size_t> embedCounter_;
  static std::atomic<size_t> errorCounter_;

  static std::chrono::steady_clock::time_point startTime_;

  // Moving averages for performance
  static std::atomic<double> avgSearchTimeMs_;
  static std::atomic<double> avgChatTimeMs_;
  static std::atomic<double> avgEmbedTimeMs_;
};

std::atomic<size_t> HttpServer::Impl::requestCounter_{ 0 };
std::atomic<size_t> HttpServer::Impl::searchCounter_{ 0 };
std::atomic<size_t> HttpServer::Impl::chatCounter_{ 0 };
std::atomic<size_t> HttpServer::Impl::embedCounter_{ 0 };
std::atomic<size_t> HttpServer::Impl::errorCounter_{ 0 };
std::chrono::steady_clock::time_point HttpServer::Impl::startTime_;
std::atomic<double> HttpServer::Impl::avgSearchTimeMs_{ 0.0 };
std::atomic<double> HttpServer::Impl::avgChatTimeMs_{ 0.0 };
std::atomic<double> HttpServer::Impl::avgEmbedTimeMs_{ 0.0 };


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
  auto &auth = imp->app_.auth();

  server.set_mount_point("/", "./public/");

  server.Get("/", [this](const httplib::Request &, httplib::Response &res) {
    LOG_MSG << "GET /";
    if (!std::filesystem::exists(imp->app_.settings().configPath())) {
      res.set_redirect("/setup/");
    } else {
      res.set_content(R"(
                <h1>PhenixCode Embedder</h1>
                <p>API is running!</p>
                <ul>
                    <li><a href="/api/health">Health Check</a></li>
                    <li><a href="/api/stats">Statistics</a></li>
                    <li><a href="/api/metrics">Metrics</a></li>
                    <li><a href="/setup/">Setup Wizard</a></li>
                </ul>
            )", "text/html");
    }
    Impl::requestCounter_++;
    });

  server.Post("/api/authenticate", [&](const httplib::Request &req, httplib::Response &res) {
    LOG_MSG << "POST /api/authenticate";
    std::string jwt;
    if (!requireAuth(auth, req, res, &jwt)) return; // Validate password
    json response = { 
      {"status", "OK"},
      {"token", jwt}
    };
    res.set_content(response.dump(), "application/json");
    Impl::requestCounter_++;
    });

  server.Post("/api/setup", [&](const httplib::Request &req, httplib::Response &res) {
    LOG_MSG << "POST /api/setup";
    if (!requireAuth(auth, req, res, nullptr)) return;
    try {
      json config = json::parse(req.body);
      // Loosely validate config
      if (!config.contains("embedding")) {
        throw std::invalid_argument("Missing embedding field");
      }
      if (!config.contains("generation")) {
        throw std::invalid_argument("Missing generation field");
      }
      if (!config.contains("database")) {
        throw std::invalid_argument("Missing database field");
      }
      if (!config.contains("chunking")) {
        throw std::invalid_argument("Missing chunking field");
      }
      auto &settings = imp->app_.refSettings();
      settings.updateFromConfig(config);
      settings.save();
      json response = {
          {"status", "success"},
          {"message", "Configuration generated successfully"}
      };
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    Impl::requestCounter_++;
    });

  server.Get("/api/setup", [&](const httplib::Request &req, httplib::Response &res) {
    LOG_MSG << "GET /api/setup";
    if (!requireAuth(auth, req, res, nullptr)) return;
    try {
      const auto &config = imp->app_.settings().configDump();
      res.set_content(config, "application/json");
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
      Impl::errorCounter_++;
    }
    Impl::requestCounter_++;
    });

  server.Get("/api/health", [](const httplib::Request &, httplib::Response &res) {
    LOG_MSG << "GET /api/health";
    json response = { {"status", "ok"} };
    res.set_content(response.dump(), "application/json");
    Impl::requestCounter_++;
    });

  server.Post("/api/search", [this](const httplib::Request &req, httplib::Response &res) {
    const auto start = std::chrono::steady_clock::now();
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
      Impl::errorCounter_++;
    }
    Impl::requestCounter_++;
    Impl::searchCounter_++;
    recordDuration(start, Impl::avgSearchTimeMs_);
    });

  // (one-off embedding without storage)
  server.Post("/api/embed", [this](const httplib::Request &req, httplib::Response &res) {
    const auto start = std::chrono::steady_clock::now();
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
      Impl::errorCounter_++;
    }
    Impl::requestCounter_++;
    Impl::embedCounter_++;
    recordDuration(start, Impl::avgEmbedTimeMs_);
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
      Impl::errorCounter_++;
    }
    Impl::requestCounter_++;
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
    Impl::requestCounter_++;
    });

  server.Get("/api/stats", [this](const httplib::Request &, httplib::Response &res) {
    try {
      LOG_MSG << "GET /api/stats";
      auto stats = imp->app_.db().getStats();
      json sources_obj = json::object();
      //for (const auto &[source, count] : stats.sources) {
      //  sources_obj[source] = count;
      //}
      json response = {
          {"total_chunks", stats.totalChunks},
          {"vector_count", stats.vectorCount},
          {"sources", imp->app_.sourceStats()}
      };
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
      Impl::errorCounter_++;
    }
    Impl::requestCounter_++;
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
      Impl::errorCounter_++;
    }
    Impl::requestCounter_++;
    });

  server.Post("/api/chat", [this](const httplib::Request &req, httplib::Response &res) {
    const auto start = std::chrono::steady_clock::now();
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
        addToSearchResult(attachmentResults, att.filename.empty() ? "attachment" : att.filename, att.content);
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
        vecAddIfUnique(relSources, relations);
        vecAddIfUnique(allFullSources, relations);
      }

      for (const auto &src : sources) {
        addToSearchResult(fullSourceResults, src, imp->app_.sourceProcessor().fetchSource(src).content);
      }
      for (const auto &rel : relSources) {
        addToSearchResult(relatedSrcResults, rel, imp->app_.sourceProcessor().fetchSource(rel).content);
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
            std::set<std::string> distinctSources;
            for (const auto &result : orderedResults) {
              if (distinctSources.insert(result.sourceId).second) {
                sourcesJson.push_back(result.sourceId);
              }
            }

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
        Impl::errorCounter_++;
      }
      Impl::requestCounter_++;
      Impl::chatCounter_++;
      recordDuration(start, Impl::avgChatTimeMs_);
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
        Impl::errorCounter_++;
      }
      HttpServer::Impl::requestCounter_++;
      });

    // Add to your HTTP server
    server.Get("/api/metrics", [this](const httplib::Request &, httplib::Response &res) {
      auto &app = imp->app_;
      auto stats = app.db().getStats();

      json metrics = {
          {"service", {
              {"version", "1.0.0"},
              {"uptime_seconds", app.uptimeSeconds()},
              {"started_at", app.startTimestamp()}
          }},
          {"database", {
              {"total_chunks", stats.totalChunks},
              {"vector_count", stats.vectorCount},
              {"deleted_count", stats.deletedCount},
              {"active_count", stats.activeCount},
              {"db_size_mb", app.dbSizeMB()},
              {"index_size_mb", app.indSizeMB()}
          }},
          {"requests", {
              {"total", Impl::requestCounter_.load()},
              {"search", Impl::searchCounter_.load()},
              {"chat", Impl::chatCounter_.load()},
              {"embed", Impl::embedCounter_.load()},
              {"errors", Impl::errorCounter_.load()}
          }},
          {"performance", {
              {"avg_search_ms", Impl::avgSearchTimeMs_.load()},
              {"avg_embedding_ms", Impl::avgEmbedTimeMs_.load()},
              {"avg_chat_ms", Impl::avgChatTimeMs_.load()}
          }},
          {"system", {
              {"last_update", app.lastUpdateTimestamp()},
              {"sources_indexed", stats.sources.size()}
          }}
      };
      res.set_content(metrics.dump(2), "application/json");
      Impl::requestCounter_++;
      });

    server.Get("/metrics", [this](const httplib::Request &, httplib::Response &res) {
      std::stringstream prometheus;

      // Request counters
      prometheus << "# HELP embedder_requests_total Total requests\n";
      prometheus << "# TYPE embedder_requests_total counter\n";
      prometheus << "embedder_requests_total " << Impl::requestCounter_ << "\n\n";

      prometheus << "# HELP embedder_search_requests_total Total search requests\n";
      prometheus << "# TYPE embedder_search_requests_total counter\n";
      prometheus << "embedder_search_requests_total " << Impl::searchCounter_ << "\n\n";

      prometheus << "# HELP embedder_chat_requests_total Total chat requests\n";
      prometheus << "# TYPE embedder_chat_requests_total counter\n";
      prometheus << "embedder_chat_requests_total " << Impl::chatCounter_ << "\n\n";

      prometheus << "# HELP embedder_embed_requests_total Total embedding requests\n";
      prometheus << "# TYPE embedder_embed_requests_total counter\n";
      prometheus << "embedder_embed_requests_total " << Impl::embedCounter_ << "\n\n";

      prometheus << "# HELP embedder_error_requests_total Total error requests\n";
      prometheus << "# TYPE embedder_error_requests_total counter\n";
      prometheus << "embedder_error_requests_total " << Impl::errorCounter_ << "\n\n";

      // Performance metrics (moving averages)
      prometheus << "# HELP embedder_avg_search_time_ms Average search time in milliseconds\n";
      prometheus << "# TYPE embedder_avg_search_time_ms gauge\n";
      prometheus << "embedder_avg_search_time_ms " << Impl::avgSearchTimeMs_.load() << "\n\n";

      prometheus << "# HELP embedder_avg_chat_time_ms Average chat time in milliseconds\n";
      prometheus << "# TYPE embedder_avg_chat_time_ms gauge\n";
      prometheus << "embedder_avg_chat_time_ms " << Impl::avgChatTimeMs_.load() << "\n\n";

      prometheus << "# HELP embedder_avg_embed_time_ms Average embedding time in milliseconds\n";
      prometheus << "# TYPE embedder_avg_embed_time_ms gauge\n";
      prometheus << "embedder_avg_embed_time_ms " << Impl::avgEmbedTimeMs_.load() << "\n\n";

      // Database metrics
      try {
        auto stats = imp->app_.db().getStats();
        prometheus << "# HELP embedder_database_chunks_total Total chunks in database\n";
        prometheus << "# TYPE embedder_database_chunks_total gauge\n";
        prometheus << "embedder_database_chunks_total " << stats.totalChunks << "\n\n";

        prometheus << "# HELP embedder_database_vectors_total Total vectors in database\n";
        prometheus << "# TYPE embedder_database_vectors_total gauge\n";
        prometheus << "embedder_database_vectors_total " << stats.vectorCount << "\n\n";

        prometheus << "# HELP embedder_database_sources_total Total sources in database\n";
        prometheus << "# TYPE embedder_database_sources_total gauge\n";
        prometheus << "embedder_database_sources_total " << stats.sources.size() << "\n\n";
      } catch (const std::exception &e) {
        prometheus << "# Database metrics unavailable: " << e.what() << "\n\n";
      }

      res.set_content(prometheus.str(), "text/plain");
      Impl::requestCounter_++;
      });

    server.Get("/api", [](const httplib::Request &, httplib::Response &res) {
      LOG_MSG << "GET /api";
      json info = {
          {"name", "Embeddings RAG API"},
          {"version", "1.0.0"},
          {"endpoints", {
              {"GET /api/setup", "Fetch setup configuration"},
              {"GET /api/health", "Health check"},
              {"GET /api/documents", "Get documents"},
              {"GET /api/stats", "Database statistics"},
              {"GET /api/settings", "Available APIs"},
              {"GET /api/metrics", "Service and database metrics"},
              {"GET /metrics", "Prometheus-compatible metrics"},
              {"POST /api/setup", "Setup configuration"},
              {"POST /api/search", "Semantic search"},
              {"POST /api/chat", "Chat with context (streaming)"},
              {"POST /api/embed", "Generate embeddings"},
              {"POST /api/documents", "Add documents"},
              {"POST /api/update", "Trigger manual update"},
          }}
      };
      res.set_content(info.dump(2), "application/json");
      HttpServer::Impl::requestCounter_++;
      });

    LOG_MSG << "Starting HTTP API server on port " << port << "...";
    LOG_MSG << "\nEndpoints:";
    LOG_MSG << "  GET  /api";
    LOG_MSG << "  GET  /metrics       - Prometheus-compatible format";
    LOG_MSG << "  GET  /api/metrics";
    LOG_MSG << "  GET  /api/setup";
    LOG_MSG << "  GET  /api/health";
    LOG_MSG << "  GET  /api/stats";
    LOG_MSG << "  GET  /api/settings";
    LOG_MSG << "  GET  /api/documents";
    LOG_MSG << "  POST /api/setup     - {\"...\"}";
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
