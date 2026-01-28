#pragma once

#include "NodeConfig.hpp"
#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

struct cacheEntry {
  std::string payload;
  // todo: trecere la steady_clock?
  // primit de la NodeConfig
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
  // race condition
  mutable std::mutex threadSafetyMutex;
  // lista dublu inlantuita care tine minte ordinea fisierelor folosite. front -
  // VIP access, back - LRU si candidat la stergere
  std::list<std::string> listaLRU;
  // mapKey - denumirea fisierului ("video.mp4")
  // mapValue - pereche de list::iterator, pentru a-l muta in fata si CacheEntry
  // pentru a obtine datele efective, ca si sizeBytes si cacheValue
  using LruIterator = std::list<std::string>::iterator;
  std::unordered_map<std::string, std::pair<LruIterator, cacheEntry>> lookupMap;
  // for debugging
  // mutable cacheStats stats;
public:
  explicit cacheStore(const NodeConfig &config);
  std::unique_ptr<std::string> Get(const std::string &key);
  void Put(const std::string &key, std::string &value);
  void Remove(const std::string &key);
};

// ENTRY1 <-> ENTRY2 <-> ENTRY3
// FRONT                 BACK
