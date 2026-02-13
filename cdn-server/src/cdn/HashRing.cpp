#include "cdn/HashRing.hpp"
#include "cdn/NodeConfig.hpp"
#include "cdn/Types.hpp"
#include "miscellaneous/ErrorHandling.hpp"
#include <algorithm>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <string_view>

std::size_t HasheazaResursa(std::string_view text) {
  constexpr std::size_t fnv_offset = 14695981039346656037ULL;
  constexpr std::size_t fnv_prime = 1099511628211ULL;
  std::size_t hash = fnv_offset;
  for (const unsigned char c : text) {
    hash ^= c;
    hash *= fnv_prime;
  }
  return hash;
}

static bool compareById(const PeerDescriptor &a, const PeerDescriptor &b) {
  return a.ID < b.ID;
}

hashRing::hashRing(const NodeConfig &config)
    : serviceNodes(config.peersVector) {
  throwIF(serviceNodes.empty(), "Hashring necesita macar un nod");

  std::ranges::sort(serviceNodes, compareById);
}

PeerDescriptor hashRing::Locate(std::string_view resursa) const {
  std::shared_lock lock(threadMutex);
  const std::size_t valoareHash = HasheazaResursa(resursa);
  const std::size_t index = valoareHash % serviceNodes.size();
  return serviceNodes[index];
}

PeerDescriptor hashRing::NextAfter(const std::string &idNod) const {
  std::shared_lock lock(threadMutex);
  if (serviceNodes.size() <= 1)
    return PeerDescriptor{};
  for (std::size_t i = 0; i < serviceNodes.size(); i++) {
    if (serviceNodes[i].ID == idNod) {
      return serviceNodes[(i + 1) % serviceNodes.size()];
    }
  }
  return serviceNodes[0];
}

void hashRing::AddNode(const PeerDescriptor &node) {
  std::unique_lock lock(threadMutex);
  for (const auto &n : serviceNodes) {
    if (n.ID == node.ID)
      return;
  }

  serviceNodes.push_back(node);
  std::ranges::sort(serviceNodes, compareById);
}

std::vector<PeerDescriptor> hashRing::Nodes() const {
  std::shared_lock lock(threadMutex);
  return serviceNodes;
}