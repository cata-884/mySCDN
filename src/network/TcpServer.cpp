#include "network/TcpServer.hpp"
#include "network/TcpSocket.hpp"

void TcpServer::Start(const std::string& ipAdress, std::uint16_t port, int backlog){
    socketAscultare = TcpSocket{};
    socketAscultare.Bind(port, ipAdress);
    socketAscultare.Listen(backlog);
}
TcpSocket TcpServer::Accept() const{
    return socketAscultare.Accept();
}