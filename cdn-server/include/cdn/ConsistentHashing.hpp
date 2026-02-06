#pragma once

#include <shared_mutex>

#include "cdn/Types.hpp"
#include <openssl/md5.h>
#include "cdn/NodeConfig.hpp"
#include <string>
#include <vector>

struct RingEntry {
  size_t m_hash{};
  PeerDescriptor m_peer;

  bool operator<(const RingEntry& other) const {
    return m_hash < other.m_hash;
  }
  bool operator<(const size_t val) const {
    return m_hash < val;
  }
};

class ConsistentHashing {

public:
  explicit ConsistentHashing(int virtualNodes, const NodeConfig& config);
  void addNode(const PeerDescriptor& peer);
  void removeNode(const std::string& node);
  PeerDescriptor getNode(const std::string& key);


private:
  int m_virtual_nodes;
  std::vector<RingEntry> m_ring;
  mutable std::shared_mutex m_mutex;
};