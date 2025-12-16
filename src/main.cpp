#include "cdn/NodeConfig.hpp"
#include "cdn/NodeServer.hpp"
#include "network/TcpServer.hpp"
#include "miscellaneous/ThreadPool.hpp"
#include <iostream>
#include <memory>
#include<signal.h>

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    try {
        NodeConfig config = ParseArguments(argc, argv);
        NodeServer cdnNode(config);
        TcpServer tcpServer;
        tcpServer.Start(config.ipAddress, config.port);

        ThreadPool pool(4);

        std::cout << "Nodul " << config.nodeId << " ruleaza..." << std::endl;

        while(1) {
            TcpSocket clientSocket = tcpServer.Accept();
            auto socketPtr = std::make_shared<TcpSocket>(std::move(clientSocket));
            pool.Enqueue([&cdnNode, socketPtr]() {
                cdnNode.HandleConnection(std::move(*socketPtr));
            });
        }

    } catch (const std::exception& e) {
        std::cerr << "Eroare: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}