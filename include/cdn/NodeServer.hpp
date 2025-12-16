#pragma once

#include "cdn/Cache.hpp"
#include "cdn/HashRing.hpp"
#include "cdn/LoadMonitor.hpp"
#include "cdn/NodeConfig.hpp"
#include "network/TcpSocket.hpp"

#include <mutex>
#include <memory> 

class NodeServer {
private:
    NodeConfig m_config;
    // Componentele interne
    cacheStore m_cache;
    hashRing m_ring;
    
    std::shared_ptr<loadMonitor> m_monitor;
    std::mutex m_logMutex;
    void Log(const std::string& message);
    void ResolveContent(const std::string& resursa, TcpSocket& client);
    std::unique_ptr<std::string> FetchFromPeer(const PeerDescriptor& peer, const std::string& resursa);
    std::unique_ptr<std::string> FetchFromOrigin(const std::string& resursa);
    void JoinCluster();
public:
    explicit NodeServer(NodeConfig config);
    void HandleConnection(TcpSocket client);

    const NodeConfig& GetConfig() const { return m_config; }
};