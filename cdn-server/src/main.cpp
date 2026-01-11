#include "cdn/NodeConfig.hpp"
#include "cdn/NodeServer.hpp"
#include "miscellaneous/ThreadPool.hpp"
#include "network/TcpServer.hpp"
#include "network/TcpSocket.hpp"
#include <iostream>
#include <memory>
#include <signal.h>

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  try {
    NodeConfig config = ParseArguments(argc, argv);
    NodeServer cdnNode(config);
    TcpServer tcpServer;
    tcpServer.Start(config.ipAddress, config.port);
    // aflam cat threaduri putem folosi
    unsigned int threaduriPosibile = std::thread::hardware_concurrency();
    size_t poolSize = (threaduriPosibile > 0) ? threaduriPosibile * 2 : 20;
    ThreadPool pool(poolSize);

    std::cout << "Nodul " << config.nodeId << " ruleaza..." << std::endl;
    std::cout << "   -> Max Conexiuni Logice: " << config.maxConexiuni
              << std::endl;
    std::cout << "   -> Thread Pool Size: " << poolSize << std::endl;

    while (1) {
      TcpSocket clientSocket = tcpServer.Accept();
      // folosim shared pointer deoarece std::function cere ca tot ce pui in el
      // sa fie copiabil, lucru pe care TcpSocket nu il are(putem face doar
      // move)
      std::shared_ptr<TcpSocket> socketPtr =
          std::make_shared<TcpSocket>(std::move(clientSocket));
      pool.Enqueue([&cdnNode, socketPtr]() {
        cdnNode.StartClientLoop(std::move(*socketPtr), nullptr);
      });
    }

  } catch (const std::exception &e) {
    std::cerr << "Eroare: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}