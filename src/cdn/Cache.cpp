#include "cdn/Cache.hpp"
#include "cdn/NodeConfig.hpp"

#include <chrono>
#include <mutex>
#include <memory>


cacheStore::cacheStore(const NodeConfig& config) 
    : maxCapacityBytes(config.cacheSize), currentUsageBytes(0), defaultTTL(config.ttlImplicit) { }

std::unique_ptr<std::string> cacheStore::Get(const std::string& key) {
    //nu pot opera 2+ threaduri in acelas timp
    std::unique_lock<std::mutex> lock(threadSafetyMutex);
    auto it = lookupMap.find(key);

    if(it == lookupMap.end()){
        //++stats.misses;
        return nullptr;
    }

    auto now = std::chrono::system_clock::now();
    cacheEntry& entry = it->second.second;
    //a expirat termenul fisierului din subNodes
    if(now > entry.expiryTime) {
        listaLRU.erase(it->second.first);
        currentUsageBytes -= entry.sizeInBytes;
        lookupMap.erase(it);
        //++stats.misses;
        return nullptr;
    }
    //mutam elementul most recently used in fata 
    listaLRU.splice(listaLRU.begin(), listaLRU, it->second.first);
    //++stats.hits;
    return std::unique_ptr<std::string>(new std::string(entry.payload));
}

void cacheStore::Put(const std::string& key, std::string& value){
    const auto valueSize = value.size();
        if(valueSize>maxCapacityBytes){
        return;
    }
    std::lock_guard<std::mutex> lock(threadSafetyMutex);
    auto now = std::chrono::system_clock::now();
    auto expiry = now + defaultTTL;
    auto it = lookupMap.find(key);
    //daca fisierul s-a gasit, il stergem
    if(it != lookupMap.end()){
        currentUsageBytes-=it->second.second.sizeInBytes;
        listaLRU.splice(listaLRU.begin(), listaLRU, it->second.first);
        it->second.second = cacheEntry(std::move(value), expiry, valueSize);
    }
    else{
        listaLRU.push_front(key);
        lookupMap.emplace(key, std::make_pair(listaLRU.begin(), cacheEntry(std::move(value), expiry, valueSize)));
    }
    currentUsageBytes += valueSize;
    //stergem componente extra pentru a face loc pentru noul component
    while(currentUsageBytes > maxCapacityBytes - valueSize && !listaLRU.empty()){
        //extragem ultimul element
        const auto& lruKey = listaLRU.back();
        auto lruIT = lookupMap.find(lruKey);
        if(lruIT != lookupMap.end()){
            currentUsageBytes-=lruIT->second.second.sizeInBytes;
            lookupMap.erase(lruIT);
            //++stats.evictions;
        }
        listaLRU.pop_back();
    }
}
/*cacheStats cacheStore::Stats() const {
    std::lock_guard<std::mutex> lock(threadSafetyMutex);
    return stats;
}*/

void cacheStore::Remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(threadSafetyMutex);
    auto it = lookupMap.find(key);
    if (it != lookupMap.end()) {
        currentUsageBytes -= it->second.second.sizeInBytes;
        listaLRU.erase(it->second.first);
        lookupMap.erase(it);
    }
}