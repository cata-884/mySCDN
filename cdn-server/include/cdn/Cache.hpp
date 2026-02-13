#pragma once

#include "NodeConfig.hpp"
#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>

struct cacheEntry {
  std::string payload;

  std::chrono::system_clock::time_point expiryTime;
  std::size_t sizeInBytes;
  cacheEntry() : sizeInBytes(0) {}

  cacheEntry(std::string p, const std::chrono::system_clock::time_point t,
             const std::size_t s)
      : payload(std::move(p)), expiryTime(t), sizeInBytes(s) {}
};

class cacheStore {
private:
  std::size_t maxCapacityBytes;
  std::size_t currentUsageBytes;
  std::chrono::seconds defaultTTL;

  mutable std::shared_mutex threadSafetyMutex;

  std::list<std::string> listaLRU;

  using LruIterator = std::list<std::string>::iterator;
  std::unordered_map<std::string, std::pair<LruIterator, cacheEntry>> lookupMap;

public:
  explicit cacheStore(const NodeConfig &config);
  std::unique_ptr<std::string> Get(const std::string &key);
  void Put(const std::string &key, std::string &value);
  void Remove(const std::string &key);
};
