#pragma once

#include "cdn/Types.hpp"

#include "cdn/NodeConfig.hpp"
#include <mutex>
#include <string>
#include <vector>

class hashRing {
private:
  std::vector<PeerDescriptor> serviceNodes;
  mutable std::mutex threadMutex;

public:
  explicit hashRing(const NodeConfig &config);
  PeerDescriptor Locate(const std::string &resursa) const;
  PeerDescriptor NextAfter(const std::string &idNod) const;
  std::vector<PeerDescriptor> Nodes() const;
  void AddNode(const PeerDescriptor &node);
};