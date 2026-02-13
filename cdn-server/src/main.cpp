#include "cdn/NodeConfig.hpp"
#include "cdn/NodeServer.hpp"
#include "miscellaneous/ThreadPool.hpp"
#include "network/TcpServer.hpp"
#include "network/TcpSocket.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

static std::atomic<bool> g_running{true};
static int g_listenFd = -1;

static void shutdown_handler(int) {
  g_running.store(false, std::memory_order_relaxed);
  if (g_listenFd >= 0) {
    ::shutdown(g_listenFd, SHUT_RDWR);
  }
}

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  try {
    NodeConfig config = ParseArguments(argc, argv);
    NodeServer cdnNode(config);
    TcpServer tcpServer;
    tcpServer.Start(config.ipAddress, config.port);

    g_listenFd = tcpServer.getSockFD();

    struct sigaction sa{};
    sa.sa_handler = shutdown_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    unsigned int threaduriPosibile = std::thread::hardware_concurrency();
    size_t poolSize = threaduriPosibile > 0 ? threaduriPosibile * 2 : 20;
    ThreadPool pool(poolSize);

    std::cout << "Nodul " << config.nodeId << " ruleaza..." << std::endl;

    while (g_running.load(std::memory_order_relaxed)) {
      try {
        TcpSocket clientSocket = tcpServer.Accept();

        auto socketPtr = std::make_shared<TcpSocket>(std::move(clientSocket));

        pool.Enqueue([&cdnNode, socketPtr]() {
          cdnNode.StartClientLoop(std::move(*socketPtr), nullptr);
        });
      } catch (...) {
        if (!g_running.load(std::memory_order_relaxed))
          break;
        throw;
      }
    }

    std::cout << "\n[SIGNAL] Oprire ceruta. Execut GracefulShutdown...\n";
    cdnNode.GracefulShutdown();
    std::cout << "[SIGNAL] Shutdown complet. La revedere.\n";
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Eroare: " << e.what() << std::endl;
    return 1;
  }
}