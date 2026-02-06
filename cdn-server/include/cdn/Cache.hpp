#pragma once

#include "cdn/NodeConfig.hpp"
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>
#include <string>

namespace cdn {

  using Clock = std::chrono::steady_clock;

  struct CacheEntry {
    std::shared_ptr<std::string> m_payload;
    Clock::time_point m_expiry_time;
    std::size_t m_payload_size;

    CacheEntry(const std::shared_ptr<std::string> &p, const Clock::time_point t, const std::size_t s) : m_payload(p),
    m_expiry_time(t), m_payload_size(s) {}
  };

  struct alignas(64) CacheShard {
    public:
    explicit CacheShard(std::size_t capacity);

    std::shared_ptr<std::string> get(const std::string& key);
    void put(const std::string& key, const std::string& data, std::chrono::seconds ttl);

    private:
    std::size_t maxCapacityBytes;
    std::size_t currentUsageBytes;
    std::size_t protectedCapacity;

    mutable std::mutex shardMutex;

    std::list<std::string> probationList;
    std::list<std::string> protectedList;

    struct MapValue {
      std::list<std::string>::iterator listIt;
      bool isProtected{};
      CacheEntry entry;
    };

    std::unordered_map<std::string, MapValue> lookup;

    void remove_internal(std::unordered_map<std::string, MapValue>::iterator it);
    void evict_if_needed();
  };

  class HybridCache {
  public:
    explicit HybridCache(const NodeConfig& config, std::size_t shardsCount = 64);

    std::shared_ptr<std::string> Get(const std::string& key);
    void Put(const std::string& key, const std::string& value);

  private:
    std::vector<std::unique_ptr<CacheShard>> shards;
    std::size_t numShards;
    std::chrono::seconds defaultTTL;

    [[nodiscard]] CacheShard& get_shard(const std::string& key) const;
  };

}