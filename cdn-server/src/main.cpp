#include "cdn/NodeConfig.hpp"
#include "cdn/NodeServer.hpp"
#include "network/TcpListener.hpp"
#include "network/TcpSocket.hpp"
#include <iostream>
#include <memory>
#include <csignal>
#include <thread>

void pin_thread_to_core(const unsigned int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

int main(const int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  try {
    const NodeConfig config = ParseArguments(argc, argv);
    const unsigned int num_cores = std::thread::hardware_concurrency();
    std::vector<std::thread> workers;

    for (unsigned int i=0; i < num_cores; i++) {
      workers.emplace_back([config, i] {
        pin_thread_to_core(i);
        NodeServer worker(config);
        worker.RunEventLoop();
      });
    }
    for (auto& t : workers) {
      if (t.joinable()) t.join();
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}