#include "database.h"
#include <hnswlib/hnswlib.h>
#include <sqlite3.h>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <format>
#include "app.h"
#include "utils_log/logger.hpp"


namespace {

  struct SqliteErrorChecker {
    sqlite3 *sq_ = nullptr;
    SqliteErrorChecker &operator = (sqlite3 *sq) { sq_ = sq; return *this; }
    SqliteErrorChecker &operator = (int rc) {
      if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error("SQLite error: " + std::string(sqlite3_errmsg(sq_)));
      }
      return *this;
    }
  };

  SqliteErrorChecker _checkErr;

} // anonymous namespace


struct HnswSqliteVectorDatabase::Impl {
  std::unique_ptr<hnswlib::HierarchicalNSW<float>> index_;
  std::unique_ptr<hnswlib::SpaceInterface<float>> space_;
  
  DistanceMetric metric_;

  sqlite3 *db_ = nullptr;

  size_t vectorDim_ = 0;
  size_t maxElements_ = 0;
  std::string dbPath_;
  std::string indexPath_;
};


HnswSqliteVectorDatabase::HnswSqliteVectorDatabase(
  const std::string &dbPath, const std::string &indexPath, size_t vectorDim, size_t maxElements, VectorDatabase::DistanceMetric metric)
  : imp(new Impl)
{
  imp->metric_ = metric;
  imp->dbPath_ = dbPath;
  imp->indexPath_ = indexPath;
  imp->vectorDim_ = vectorDim;
  imp->maxElements_ = maxElements;

  initializeDatabase();
  initializeVectorIndex();
}

HnswSqliteVectorDatabase::~HnswSqliteVectorDatabase() {
  if (imp->db_) {
    sqlite3_close(imp->db_);
    _checkErr = nullptr;
  }
}

size_t HnswSqliteVectorDatabase::addDocument(const Chunk &chunk, const std::vector<float> &embedding)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (embedding.size() != imp->vectorDim_) {
    throw std::runtime_error(std::format("Embedding dimension mismatch: actual {}, claimed {}", embedding.size(), imp->vectorDim_));
  }
  size_t chunkId = insertMetadata(chunk);
  try {
    upsertFileMetadata(chunk.docUri, utils::getFileModificationTime(chunk.docUri), std::filesystem::file_size(chunk.docUri));
  } catch (const std::exception &) {
  }
  imp->index_->addPoint(embedding.data(), chunkId);
  return chunkId;
}

std::vector<size_t> HnswSqliteVectorDatabase::addDocuments(const std::vector<Chunk> &chunks, const std::vector<std::vector<float>> &embeddings)
{
  if (chunks.size() != embeddings.size()) {
    throw std::runtime_error("Chunks and embeddings count mismatch");
  }
  std::vector<size_t> chunkIds;
  for (size_t i = 0; i < chunks.size(); ++i) {
    size_t id = addDocument(chunks[i], embeddings[i]);
    chunkIds.push_back(id);
  }
  return chunkIds;
}

std::vector<SearchResult> HnswSqliteVectorDatabase::search(const std::vector<float> &queryEmbedding, size_t topK) const
{
  if (queryEmbedding.size() != imp->vectorDim_) {
    throw std::runtime_error(std::format("Query embedding dimension mismatch: actual {}, claimed {}", queryEmbedding.size(), imp->vectorDim_));
  }
  if (imp->index_->getCurrentElementCount() == 0) {
    return {};
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto result = imp->index_->searchKnn(queryEmbedding.data(), topK);
  std::vector<SearchResult> searchResults;
  while (!result.empty()) {
    const auto [distance, label] = result.top();
    result.pop();

    float similarity = 0;
    if (imp->metric_ == DistanceMetric::Cosine) {
      // InnerProduct returns negative dot product
      // For normalized vectors: similarity = (1 + dot_product) / 2
      // Or simply: similarity = -distance (if vectors normalized to [-1,1])
      similarity = 1.0f - distance; // Higher = more similar
    } else {
      // L2 distance
      similarity = 1.0f / (1.0f + distance);
    }

    auto chunkData = getChunkMetadata(label);
    if (chunkData.has_value()) {
      SearchResult sr = chunkData.value();
      sr.similarityScore = similarity;
      sr.chunkId = label;
      sr.distance = distance;
      searchResults.push_back(sr);
    }
  }
  std::sort(searchResults.begin(), searchResults.end(),
    [](const SearchResult &a, const SearchResult &b) {
      return a.similarityScore > b.similarityScore;
    });
  return searchResults;
}

std::vector<SearchResult> HnswSqliteVectorDatabase::searchWithFilter(const std::vector<float> &queryEmbedding,
  const std::string &sourceFilter,
  const std::string &typeFilter,
  size_t topK) const
{
  auto results = search(queryEmbedding, topK * 2);
  std::vector<SearchResult> filtered;
  for (const auto &result : results) {
    bool matches = true;
    if (!sourceFilter.empty() && result.sourceId.find(sourceFilter) == std::string::npos) {
      matches = false;
    }
    if (!typeFilter.empty() && result.chunkType != typeFilter) {
      matches = false;
    }
    if (matches) {
      filtered.push_back(result);
      if (filtered.size() >= topK) break;
    }
  }
  return filtered;
}

void HnswSqliteVectorDatabase::clear()
{
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    beginTransaction();
    executeSql("DELETE FROM chunks");
    executeSql("DELETE FROM files_metadata");
    // Just recreate index - simpler than unmarking everything
    imp->space_ = std::make_unique<hnswlib::L2Space>(imp->vectorDim_);
    imp->index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(
      imp->space_.get(), imp->maxElements_, 16, 200, 42, true
    );
  } catch (...) {
    rollback();
  }
  commit();
}

void HnswSqliteVectorDatabase::initializeDatabase()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    int rc = sqlite3_open(imp->dbPath_.c_str(), &imp->db_);
    if (rc != SQLITE_OK) {
      throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(imp->db_)));
    }
    _checkErr = imp->db_;
    const char *chunksTable = R"(
        CREATE TABLE IF NOT EXISTS chunks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            content TEXT NOT NULL,
            source_id TEXT NOT NULL,
            start_pos INTEGER NOT NULL,
            end_pos INTEGER NOT NULL,
            token_count INTEGER NOT NULL,
            unit TEXT NOT NULL,
            type TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    executeSql(chunksTable);

    const char *filesTable = R"(
        CREATE TABLE IF NOT EXISTS files_metadata (
            path TEXT PRIMARY KEY,
            last_modified INTEGER NOT NULL,
            file_size INTEGER NOT NULL,
            indexed_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    executeSql(filesTable);
  }
  auto files = getTrackedFiles();
  LOG_MSG << "Loaded metadata with" << files.size() << "files";
}

void HnswSqliteVectorDatabase::initializeVectorIndex()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (imp->metric_ == DistanceMetric::Cosine) {
    imp->space_ = std::make_unique<hnswlib::InnerProductSpace>(imp->vectorDim_);
  } else {
    imp->space_ = std::make_unique<hnswlib::L2Space>(imp->vectorDim_);
  }
  imp->space_ = std::make_unique<hnswlib::L2Space>(imp->vectorDim_);
  if (std::filesystem::exists(imp->indexPath_)) {
    try {
      imp->index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(imp->space_.get(), imp->indexPath_, false, imp->maxElements_, true);
      LOG_MSG << "Loaded index with" 
        << (imp->metric_ == DistanceMetric::Cosine ? "Cosine" : "L2") << "distance, "
        << imp->index_->getCurrentElementCount() << "total vectors, "
        << imp->index_->getDeletedCount() << "deleted";
      return;
    } catch (const std::exception &e) {
      LOG_MSG << "Failed to load existing index at" << std::filesystem::absolute(indexPath()) << "|" << e.what();
      LOG_MSG << "Creating new index...";
    }
  }
  imp->index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(imp->space_.get(), imp->maxElements_, 16, 200, 42, true);
}

void HnswSqliteVectorDatabase::executeSql(const std::string &sql)
{
  char *errorMessage = nullptr;
  int rc = sqlite3_exec(imp->db_, sql.c_str(), nullptr, nullptr, &errorMessage);
  if (rc != SQLITE_OK) {
    std::string error = errorMessage ? errorMessage : "Unknown error";
    if (errorMessage) sqlite3_free(errorMessage);
    throw std::runtime_error("SQL error: " + error);
  }
}

size_t HnswSqliteVectorDatabase::insertMetadata(const Chunk &chunk)
{
  const char *insertSql = R"(
        INSERT INTO chunks (content, source_id, start_pos, end_pos, token_count, unit, type)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

  sqlite3_stmt *stmt;
  _checkErr = sqlite3_prepare_v2(imp->db_, insertSql, -1, &stmt, nullptr);
  int k = 1;
  sqlite3_bind_text(stmt, k++, chunk.text.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, k++, chunk.docUri.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, k++, chunk.metadata.start);
  sqlite3_bind_int64(stmt, k++, chunk.metadata.end);
  sqlite3_bind_int64(stmt, k++, chunk.metadata.tokenCount);
  sqlite3_bind_text(stmt, k++, chunk.metadata.unit.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, k++, chunk.metadata.type.c_str(), -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to insert chunk metadata: " + std::string(sqlite3_errmsg(imp->db_)));
  }
  size_t chunkId = sqlite3_last_insert_rowid(imp->db_);
  _checkErr = sqlite3_finalize(stmt);
  return chunkId;
}

std::optional<SearchResult> HnswSqliteVectorDatabase::getChunkMetadata(size_t chunkId) const
{
  const char *selectSql = R"(
        SELECT content, source_id, unit, type, start_pos, end_pos
        FROM chunks WHERE id = ?
    )";
  sqlite3_stmt *stmt;
  _checkErr = sqlite3_prepare_v2(imp->db_, selectSql, -1, &stmt, nullptr);
  _checkErr = sqlite3_bind_int64(stmt, 1, chunkId);
  SearchResult result;
  bool found = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int k = 0;
    result.content = reinterpret_cast<const char *>(sqlite3_column_text(stmt, k++));
    result.sourceId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, k++));
    result.chunkUnit = reinterpret_cast<const char *>(sqlite3_column_text(stmt, k++));
    result.chunkType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, k++));
    result.start = sqlite3_column_int64(stmt, k++);
    result.end = sqlite3_column_int64(stmt, k++);
    found = true;
  }
  _checkErr = sqlite3_finalize(stmt);
  return found ? std::optional<SearchResult>(result) : std::nullopt;
}

std::vector<size_t> HnswSqliteVectorDatabase::getChunkIdsBySource(const std::string &sourceId) const
{
  std::vector<size_t> ids;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id FROM chunks WHERE source_id = ?";
  _checkErr = sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  _checkErr = sqlite3_bind_text(stmt, 1, sourceId.c_str(), -1, SQLITE_STATIC);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ids.push_back(sqlite3_column_int64(stmt, 0));
  }
  _checkErr = sqlite3_finalize(stmt);
  return ids;
}

size_t HnswSqliteVectorDatabase::deleteDocumentsBySource(const std::string &sourceId)
{
  std::lock_guard<std::mutex> lock(mutex_);
  auto chunkIds = getChunkIdsBySource(sourceId);
  if (chunkIds.empty()) return 0;
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM chunks WHERE source_id = ?";
  _checkErr = sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  _checkErr = sqlite3_bind_text(stmt, 1, sourceId.c_str(), -1, SQLITE_STATIC);
  _checkErr = sqlite3_step(stmt);
  size_t n = sqlite3_changes(imp->db_);
  _checkErr = sqlite3_finalize(stmt);
  for (size_t id : chunkIds) {
    imp->index_->markDelete(id);
  }
  return n;
}

void HnswSqliteVectorDatabase::removeFileMetadata(const std::string &filepath)
{
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM files_metadata WHERE path = ?";
  _checkErr = sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  _checkErr = sqlite3_bind_text(stmt, 1, filepath.c_str(), -1, SQLITE_STATIC);
  _checkErr = sqlite3_step(stmt);
  _checkErr = sqlite3_finalize(stmt);
}

void HnswSqliteVectorDatabase::upsertFileMetadata(const std::string &filepath, std::time_t mtime, size_t size)
{
  sqlite3_stmt *stmt;
  const char *sql = "INSERT OR REPLACE INTO files_metadata (path, last_modified, file_size) VALUES (?, ?, ?)";
  _checkErr = sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  _checkErr = sqlite3_bind_text(stmt, 1, filepath.c_str(), -1, SQLITE_STATIC);
  _checkErr = sqlite3_bind_int64(stmt, 2, mtime);
  _checkErr = sqlite3_bind_int64(stmt, 3, size);
  _checkErr = sqlite3_step(stmt);
  _checkErr = sqlite3_finalize(stmt);
}

std::vector<FileMetadata> HnswSqliteVectorDatabase::getTrackedFiles() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<FileMetadata> files;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT path, last_modified, file_size FROM files_metadata";
  _checkErr = sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    FileMetadata meta;
    meta.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    meta.lastModified = sqlite3_column_int64(stmt, 1);
    meta.fileSize = sqlite3_column_int64(stmt, 2);
    files.push_back(meta);
  }
  _checkErr = sqlite3_finalize(stmt);
  return files;
}

size_t HnswSqliteVectorDatabase::getChunkCountBySource(const std::string &sourceId) const
{
  sqlite3_stmt *stmt;
  const char *sql = "SELECT COUNT(*) FROM chunks WHERE source_id = ?";
  sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, sourceId.c_str(), -1, SQLITE_STATIC);
  size_t count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return count;
}

bool HnswSqliteVectorDatabase::fileExistsInMetadata(const std::string &path)
{
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt *stmt;
  const char *sql = "SELECT 1 FROM files_metadata WHERE path = ?";
  _checkErr = sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  _checkErr = sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
  bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
  _checkErr = sqlite3_finalize(stmt);
  return exists;
}

DatabaseStats HnswSqliteVectorDatabase::getStats() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  DatabaseStats stats;
  stats.vectorCount = imp->index_->getCurrentElementCount();
  stats.deletedCount = imp->index_->getDeletedCount();
  stats.activeCount = imp->index_->getCurrentElementCount() - imp->index_->getDeletedCount();
  sqlite3_stmt *stmt;
  _checkErr = sqlite3_prepare_v2(imp->db_, "SELECT COUNT(*) FROM chunks", -1, &stmt, nullptr);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    stats.totalChunks = sqlite3_column_int64(stmt, 0);
  }
  _checkErr = sqlite3_finalize(stmt);
  _checkErr = sqlite3_prepare_v2(imp->db_, "SELECT source_id, COUNT(*) FROM chunks GROUP BY source_id", -1, &stmt, nullptr);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string source = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    size_t count = sqlite3_column_int64(stmt, 1);
    stats.sources.emplace_back(source, count);
  }
  _checkErr = sqlite3_finalize(stmt);
  return stats;
}

void HnswSqliteVectorDatabase::persist()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (imp->index_->getCurrentElementCount() > 0) {
    imp->index_->saveIndex(imp->indexPath_);
    //LOG_MSG << "Saved vector index with " << imp->index_->getCurrentElementCount() << " vectors";
  } else {
    LOG_MSG << "Saving with no vectors in the index db. Skipped.";
  }
}

std::string HnswSqliteVectorDatabase::dbPath() const
{
  return imp->dbPath_;
}

std::string HnswSqliteVectorDatabase::indexPath() const
{
  return imp->indexPath_;
}

void HnswSqliteVectorDatabase::compactIndex()
{
  std::lock_guard<std::mutex> lock(mutex_);
  size_t deleted_count = imp->index_->getDeletedCount();

  if (deleted_count == 0) {
    LOG_MSG << "No deleted items to compact.";
    return;
  }

  LOG_MSG << "Compacting index (" << deleted_count << " deleted items)...";

  // Get all active chunks
  std::vector<std::pair<size_t, std::vector<float>>> activeItems;

  sqlite3_stmt *stmt;
  const char *sql = "SELECT id FROM chunks";
  _checkErr = sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    size_t chunkId = sqlite3_column_int64(stmt, 0);
    if (!imp->index_->isMarkedDeleted(chunkId)) {
      auto embedding = imp->index_->getDataByLabel<float>(chunkId);
      activeItems.emplace_back(chunkId, std::move(embedding));
    }
  }
  _checkErr = sqlite3_finalize(stmt);

  // Create new index
  auto new_space = std::make_unique<hnswlib::L2Space>(imp->vectorDim_);
  auto new_index = std::make_unique<hnswlib::HierarchicalNSW<float>>(
    new_space.get(), imp->maxElements_, 16, 200, 42, true
  );

  // Add all active items
  for (const auto &[id, embedding] : activeItems) {
    new_index->addPoint(embedding.data(), id);
  }

  // Replace old index
  imp->space_ = std::move(new_space);
  imp->index_ = std::move(new_index);

  LOG_MSG << "Compaction complete. Active items: " << imp->index_->getCurrentElementCount();
}
