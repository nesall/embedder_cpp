#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include "nlohmann/json.hpp"

class SimpleTokenizer {
  mutable std::mutex mutex_;
  mutable std::unordered_map<std::string, size_t> cache_;
private:
  nlohmann::json vocab_;
  size_t maxInputCharsPerWord_ = 100;
  size_t simulateWordpiece(const std::string &word, bool addSpecialTokens) const;
public:
  explicit SimpleTokenizer(const std::string &configPath);
  size_t estimateTokenCount(const std::string &text, bool addSpecialTokens = false) const;
  size_t countTokensWithVocab(const std::string &text, bool addSpecialTokens = false) const;
};

