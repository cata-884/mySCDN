#pragma once

#include "cdn/NodeConfig.hpp"
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

class hashRing {
private:
  std::vector<PeerDescriptor> serviceNodes;
  mutable std::shared_mutex threadMutex;

public:
  explicit hashRing(const NodeConfig &config);
  PeerDescriptor Locate(std::string_view resursa) const;
  PeerDescriptor NextAfter(const std::string &idNod) const;
  std::vector<PeerDescriptor> Nodes() const;
  void AddNode(const PeerDescriptor &node);
};