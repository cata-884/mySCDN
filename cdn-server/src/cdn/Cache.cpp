#include "cdn/Cache.hpp"
#include "cdn/NodeConfig.hpp"

#include <chrono>
#include <memory>
#include <mutex>

cacheStore::cacheStore(const NodeConfig &config)
    : maxCapacityBytes(config.cacheSize), currentUsageBytes(0),
      defaultTTL(config.ttlImplicit) {}

std::unique_ptr<std::string> cacheStore::Get(const std::string &key) {

  std::unique_lock lock(threadSafetyMutex);
  const auto it = lookupMap.find(key);

  if (it == lookupMap.end()) {

    return nullptr;
  }

  const auto now = std::chrono::system_clock::now();
  cacheEntry &entry = it->second.second;

  if (now > entry.expiryTime) {
    listaLRU.erase(it->second.first);
    currentUsageBytes -= entry.sizeInBytes;
    lookupMap.erase(it);

    return nullptr;
  }

  listaLRU.splice(listaLRU.begin(), listaLRU, it->second.first);

  return std::make_unique<std::string>(entry.payload);
}

void cacheStore::Put(const std::string &key, std::string &value) {
  const auto valueSize = value.size();
  if (valueSize > maxCapacityBytes) {
    return;
  }
  std::unique_lock lock(threadSafetyMutex);
  const auto now = std::chrono::system_clock::now();
  const auto expiry = now + defaultTTL;

  if (const auto it = lookupMap.find(key); it != lookupMap.end()) {
    currentUsageBytes -= it->second.second.sizeInBytes;
    listaLRU.splice(listaLRU.begin(), listaLRU, it->second.first);
    it->second.second = cacheEntry(std::move(value), expiry, valueSize);
  } else {
    listaLRU.push_front(key);
    lookupMap.emplace(
        key, std::make_pair(listaLRU.begin(),
                            cacheEntry(std::move(value), expiry, valueSize)));
  }
  currentUsageBytes += valueSize;

  while (currentUsageBytes > maxCapacityBytes - valueSize &&
         !listaLRU.empty()) {

    const auto &lruKey = listaLRU.back();
    if (auto lruIT = lookupMap.find(lruKey); lruIT != lookupMap.end()) {
      currentUsageBytes -= lruIT->second.second.sizeInBytes;
      lookupMap.erase(lruIT);
    }
    listaLRU.pop_back();
  }
}

void cacheStore::Remove(const std::string &key) {
  std::unique_lock lock(threadSafetyMutex);
  if (const auto it = lookupMap.find(key); it != lookupMap.end()) {
    currentUsageBytes -= it->second.second.sizeInBytes;
    listaLRU.erase(it->second.first);
    lookupMap.erase(it);
  }
}