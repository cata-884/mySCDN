#ifndef NODE_SERVER_HPP
#define NODE_SERVER_HPP

#include <liburing.h>
#include <string>
#include "cdn/NodeConfig.hpp"
#include "cdn/Database.hpp"
#include "cdn/Cache.hpp"
#include "cdn/ConsistentHashing.hpp"
#include "network/TcpSocket.hpp"


class NodeServer {
public:
    explicit NodeServer(NodeConfig c);
    ~NodeServer();
    void RunEventLoop();

private:

};

#endif