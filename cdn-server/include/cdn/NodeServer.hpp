#pragma once

#include "cdn/Cache.hpp"
#include "cdn/Database.hpp"
#include "cdn/HashRing.hpp"
#include "cdn/LoadMonitor.hpp"
#include "cdn/NodeConfig.hpp"
#include "network/TcpSocket.hpp"

#include <memory>
#include <mutex>

class NodeServer {
private:
  NodeConfig conf;
  cacheStore memoriaRam;
  hashRing inelul;
  DatabaseManager db_manager;

  std::shared_ptr<loadMonitor> monitorul;
  std::mutex logMutex;

  void log_msg(const std::string &text);
  void rezolva_continut(const std::string &nume_resursa, const TcpSocket &s,
                        const std::string &user_context);

  std::unique_ptr<std::string> iaDeLaVecin(const PeerDescriptor &vecin,
                                           const std::string &ceVreau);
  std::unique_ptr<std::string> citesteDisk(const std::string &fisier);

  void intraInCluster();

public:
  explicit NodeServer(NodeConfig c);

  loadMonitor::ticket ia_bilet() const;
  void trimiteLaAltul(const TcpSocket &clientSock); // redirect
  void StartClientLoop(TcpSocket socketul,
                       const std::shared_ptr<loadMonitor::ticket>& biletPtr = nullptr);

  const NodeConfig &get_conf() const { return conf; }
};