#pragma once

#include <map>

#include "cdn/Types.hpp"

#include "cdn/NodeConfig.hpp"
#include <string>
#include <vector>

class ConsistentHashing {

public:
  explicit ConsistentHashing(const NodeConfig &config);
  PeerDescriptor Locate(const std::string &resursa) const;
  PeerDescriptor NextAfter(const std::string &idNod) const;
  std::vector<PeerDescriptor> Nodes() const;
  void AddNode(const PeerDescriptor &node);

private:
  int m_virtualNodes;  //virtual nodes per real node
  std::map<size_t, std::string> m_ring;
};