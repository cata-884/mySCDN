#pragma once
#include "cdn/Types.hpp"
#include <chrono>
#include <string>
#include <vector>
#include <cstdint>
#include<memory>
#include <mutex>

struct NodeConfig {
    std::string nodeId;
    std::string ipAddress = "0.0.0.0";
    std::uint16_t port = 0;
    std::vector<PeerDescriptor> peersVector; //subnoduri
    std::string targetFilesLocation = "../targetFiles";
    
    std::size_t maxConexiuni = 10;
    std::size_t cacheSize = 1024 * 1024*10;
    std::chrono::seconds ttlImplicit = std::chrono::seconds(90);
    const PeerDescriptor& self() const;
    const PeerDescriptor* findNode(const std::string& id) const;
};

struct NodeConfigParseResult {
    std::unique_ptr<NodeConfig> configuratie;
    std::string eroare;
};

NodeConfig ParseArguments(int argc, char* argv[]);
