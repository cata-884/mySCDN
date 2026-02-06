#pragma once
#include "cdn/Types.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

/*
./myscdn_node \
    --node-id node1 \
    --listen 10.100.0.30:8000 \
    --advertise-addr 192.168.1.50:8000 \  # IP-ul vizibil de clienti (sau domeniu)
    --cluster-node node2@10.100.0.30:8001 \
    --target-files /mnt/storage_hdd/movies \
    --temp-path /mnt/fast_ssd/transcode_buffer \ # Pt remuxing rapid
    --max-connections 4096 \              # 10 e prea putin pt io_uring!
    --cache-bytes 2147483648 \            # 2GB RAM Cache (100MB e putin pt video)
    --workers 4 \                         # Foloseste primele 4 nuclee exclusiv
    --direct-io \                         # Bypass OS Cache (citire directa disc->net)
    --ttl 3600 \
    --db-path ../CDN.db \
    --auth-file ../auth.txt
 */
struct NodeConfig {
  std::string m_id;
  std::string m_address;
  std::uint16_t m_port{0};

  std::vector<PeerDescriptor> m_peers;

  std::string m_target;
  std::string m_db;

  std::size_t m_max_connections{1024};
  std::size_t m_cache_capacity_bytes{512 * 1024 * 1024};
  std::chrono::seconds m_ttl{3600};

  [[nodiscard]] const PeerDescriptor& self() const;
  [[nodiscard]] const PeerDescriptor* findNode(const std::string& id) const;
};

NodeConfig ParseArguments(int argc, char* argv[]);