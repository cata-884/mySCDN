#pragma once
#include "cdn/Types.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct NodeConfig {
  std::string nodeId;
  std::string ipAddress = "0.0.0.0";
  std::uint16_t port = 0;
  std::vector<PeerDescriptor> peersVector;
  std::string targetFilesLocation = "../targetFiles";
  std::string dbPath;
  std::size_t maxConexiuni = 10;
  std::size_t cacheSize = 1024 * 1024 * 10; // 10MB
  std::chrono::seconds ttlImplicit = std::chrono::seconds(90);
  [[nodiscard]] const PeerDescriptor &self() const;
  [[nodiscard]] const PeerDescriptor *findNode(const std::string &id) const;
};

struct NodeConfigParseResult {
  std::unique_ptr<NodeConfig> configuratie;
  std::string eroare;
};

NodeConfig ParseArguments(int argc, char *argv[]);