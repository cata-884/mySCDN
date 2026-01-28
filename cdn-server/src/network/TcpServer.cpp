#include "network/TcpServer.hpp"
#include "network/TcpSocket.hpp"

void TcpServer::Start(const std::string &ipAdress, const std::uint16_t port,
                      const int backlog) {
  socketAscultare = TcpSocket{};
  socketAscultare.Bind(port, ipAdress);
  socketAscultare.Listen(backlog);
}
TcpSocket TcpServer::Accept() const { return socketAscultare.Accept(); }