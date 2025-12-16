#pragma once

#include "cdn/Types.hpp"

#include <string>
#include <vector>
#include "cdn/NodeConfig.hpp"
#include<mutex>

class hashRing {
private:
    std::vector<PeerDescriptor> serviceNodes;
    mutable std::mutex threadMutex;
public:
    explicit hashRing(std::vector<PeerDescriptor> noduri);
    PeerDescriptor Locate(const std::string& resursa) const;
    PeerDescriptor NextAfter(const std::string& idNod) const;
    std::vector<PeerDescriptor> Nodes() const ;
    void AddNode(const PeerDescriptor& node);
};