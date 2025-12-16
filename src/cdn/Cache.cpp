#include "cdn/Cache.hpp"

#include <chrono>
#include <mutex>
#include <memory>


cacheStore::cacheStore(std::size_t capacity, std::chrono::seconds timeToLive) 
    : maxCapacityBytes(capacity), currentUsageBytes(0), defaultTTL(timeToLive) {

    if(maxCapacityBytes == 0){
        maxCapacityBytes = 1024 * 1024; // Default 1MB
    }
    if(defaultTTL.count() <= 0){
        defaultTTL = std::chrono::seconds(30);
    }
}

std::unique_ptr<std::string> cacheStore::Get(const std::string& key) {
    //nu pot opera 2+ threaduri in acelas timp
    std::unique_lock<std::mutex> lock(threadSafetyMutex);
    
    auto it = lookupMap.find(key);

    if(it == lookupMap.end()){
        ++stats.misses;
        return nullptr;
    }

    auto now = std::chrono::system_clock::now();
    cacheEntry& entry = it->second.second;
    //a expirat termenul fisierului din subNodes
    if(entry.expiryTime < now) {
        listaLRU.erase(it->second.first);
        currentUsageBytes -= entry.sizeInBytes;
        lookupMap.erase(it);
        ++stats.misses;
        return nullptr;
    }
    //mutam elementul most recently used in fata 
    listaLRU.splice(listaLRU.begin(), listaLRU, it->second.first);
    
    ++stats.hits;
    return std::unique_ptr<std::string>(new std::string(entry.payload));
}

void cacheStore::Put(const std::string& key, std::string& value){
    const auto valueSize = value.size();
    std::lock_guard<std::mutex> lock(threadSafetyMutex);
    if(valueSize>maxCapacityBytes){
        return;
    }
    auto it = lookupMap.find(key);
    //daca fisierul s-a gasit, il stergem
    if(it != lookupMap.end()){
        currentUsageBytes-=it->second.second.sizeInBytes;
        listaLRU.erase(it->second.first);
        lookupMap.erase(it);
    }
    //stergem componente extra pentru a face loc pentru noul component
    while(currentUsageBytes + valueSize > maxCapacityBytes){
        const auto& lruKey = listaLRU.back();
        auto lruIT = lookupMap.find(lruKey);
        if(lruIT != lookupMap.end()){
            currentUsageBytes-=lruIT->second.second.sizeInBytes;
            lookupMap.erase(lruIT);
            ++stats.evictions;
        }
        listaLRU.pop_back();
    }
    auto expirare = std::chrono::system_clock::now() + defaultTTL;
    listaLRU.push_front(key);
    cacheEntry entry(std::move(value), expirare, valueSize);
    lookupMap.emplace(key, std::make_pair(listaLRU.begin(), std::move(entry)));
    currentUsageBytes+=valueSize;
}
cacheStats cacheStore::Stats() const {
    std::lock_guard<std::mutex> lock(threadSafetyMutex);
    return stats;
}
