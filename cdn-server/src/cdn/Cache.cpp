#include "cdn/Cache.hpp"
#include <functional>

namespace cdn {

CacheEntry::CacheEntry(const std::shared_ptr<std::string> &p, const Clock::time_point t, const std::size_t s)
    : m_payload(std::move(p)), m_expiry_time(t), m_payload_size(s) {}

CacheShard::CacheShard(const std::size_t capacity)
    : maxCapacityBytes(capacity), currentUsageBytes(0) {
    protectedCapacity = static_cast<std::size_t>(0.8 * static_cast<double>(capacity));
}

std::shared_ptr<std::string> CacheShard::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(shardMutex);

    const auto it = lookup.find(key);
    if (it == lookup.end()) {
        return nullptr;
    }

    if (const auto now = Clock::now(); now > it->second.entry.m_expiry_time) {
        remove_internal(it);
        return nullptr;
    }

    if (it->second.isProtected) {
        protectedList.splice(protectedList.begin(), protectedList, it->second.listIt);
    } else {
        probationList.erase(it->second.listIt);
        protectedList.push_front(key);
        it->second.listIt = protectedList.begin();
        it->second.isProtected = true;
    }

    return it->second.entry.m_payload;
}

void CacheShard::put(const std::string& key, const std::string& data, const std::chrono::seconds ttl) {
    const std::size_t size = data.size();
    if (size > maxCapacityBytes) return;

    std::lock_guard<std::mutex> lock(shardMutex);

    const auto now = Clock::now();
    const auto expiry = now + ttl;

    if (const auto it = lookup.find(key); it != lookup.end()) {
        currentUsageBytes -= it->second.entry.m_payload_size;
        currentUsageBytes += size;

        it->second.entry.m_payload = std::make_shared<std::string>(data);
        it->second.entry.m_payload_size = size;
        it->second.entry.m_expiry_time = expiry;

        auto& list = it->second.isProtected ? protectedList : probationList;
        list.splice(list.begin(), list, it->second.listIt);

        evict_if_needed();
        return;
    }

    const auto ptr = std::make_shared<std::string>(data);
    probationList.push_front(key);

    const CacheEntry entry(ptr, expiry, size);
    lookup.insert({key, {probationList.begin(), false, entry}});

    currentUsageBytes += size;
    evict_if_needed();
}

void CacheShard::remove_internal(const std::unordered_map<std::string, MapValue>::iterator it) {
    currentUsageBytes -= it->second.entry.m_payload_size;

    if (it->second.isProtected) {
        protectedList.erase(it->second.listIt);
    } else {
        probationList.erase(it->second.listIt);
    }

    lookup.erase(it);
}

void CacheShard::evict_if_needed() {
    while (currentUsageBytes > maxCapacityBytes) {
        if (!probationList.empty()) {
            auto& key = probationList.back();
            if (auto it = lookup.find(key); it != lookup.end()) remove_internal(it);
        } else if (!protectedList.empty()) {
            auto& key = protectedList.back();
            if (auto it = lookup.find(key); it != lookup.end()) remove_internal(it);
        } else {
            break;
        }
    }
}
    HybridCache::HybridCache(const NodeConfig& config, const std::size_t shardsCount)
    : numShards(shardsCount), defaultTTL(config.m_ttl) {

    std::size_t capPerShard = config.m_cache_capacity_bytes / numShards;
    if (capPerShard == 0) capPerShard = 1024 * 1024;

    shards.reserve(numShards);
    for (std::size_t i = 0; i < numShards; ++i) {
        shards.push_back(std::make_unique<CacheShard>(capPerShard));
    }
}
    CacheShard& HybridCache::get_shard(const std::string& key) const {
    const std::size_t h = std::hash<std::string>{}(key);
    return *shards[h % numShards];
}

std::shared_ptr<std::string> HybridCache::Get(const std::string& key) {
    return get_shard(key).get(key);
}

void HybridCache::Put(const std::string& key, const std::string& value) {
    get_shard(key).put(key, value, defaultTTL);
}

}