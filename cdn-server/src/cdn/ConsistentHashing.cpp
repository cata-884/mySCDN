#include "cdn/ConsistentHashing.hpp"
#include "cdn/NodeConfig.hpp"
#include "cdn/Types.hpp"
#include <algorithm>
#include <mutex>

size_t hash(const std::string& key) {
  return std::hash<std::string>{}(key);
}
ConsistentHashing::ConsistentHashing(const int virtualNodes, const NodeConfig &config) : m_virtual_nodes(virtualNodes) {
  for (const auto& peer : config.m_peers) {
    addNode(peer);
  }
}

void ConsistentHashing::addNode(const PeerDescriptor &peer) {
  std::unique_lock lock(m_mutex);

  for (int i = 0; i < m_virtual_nodes; i++) {
    std::string v_node_key = peer.ID + "#" + std::to_string(i);
    const size_t h = hash(v_node_key);

    m_ring.push_back({h, peer});
  }

  std::sort(m_ring.begin(), m_ring.end());
}

void ConsistentHashing::removeNode(const std::string &nodeID) {
  std::unique_lock lock(m_mutex);

  const auto newEnd = std::ranges::remove_if(m_ring,
                                       [&](const RingEntry& entry) {
                                         return entry.m_peer.ID == nodeID;
                                       }).begin();

  m_ring.erase(newEnd, m_ring.end());
}

PeerDescriptor ConsistentHashing::getNode(const std::string &key) {
  std::shared_lock lock(m_mutex);

  if (m_ring.empty()) return PeerDescriptor{};

  const size_t h = hash(key);

  const auto it = std::lower_bound(m_ring.begin(), m_ring.end(), h,
      [](const RingEntry& entry, const size_t val) {
          return entry.m_hash < val;
      });

  if (it == m_ring.end()) {
    return m_ring.front().m_peer;
  }
  return it->m_peer;
}