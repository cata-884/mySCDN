#pragma once

#include "cdn/Cache.hpp"
#include "cdn/Database.hpp"
#include "cdn/HashRing.hpp"
#include "cdn/LoadMonitor.hpp"
#include "cdn/NodeConfig.hpp"
#include "network/TcpSocket.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

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
  std::unique_ptr<std::string>
  citesteDisk(const std::string &fisier,
              const std::string &folder_radacina = "");

  void intraInCluster();
  bool TransferFileToPeer(const PeerDescriptor &vecin,
                          const std::string &nume_fisier,
                          const std::string &content);
  std::string buildFilePath(const std::string &filename) const;
  std::optional<PeerDescriptor> findAnyPeer() const;
  void foreachStorageFile(const std::function<void(const std::string &)> &fn);

public:
  explicit NodeServer(NodeConfig c);

  void GracefulShutdown();
  loadMonitor::ticket ia_bilet() const;
  void trimiteLaAltul(const TcpSocket &clientSock);
  void StartClientLoop(
      TcpSocket socketul,
      const std::shared_ptr<loadMonitor::ticket> &biletPtr = nullptr);
  void RebalanceStorage();
  const NodeConfig &get_conf() const { return conf; }
};