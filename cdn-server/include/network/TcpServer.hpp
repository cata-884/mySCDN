#pragma once

#include "network/TcpSocket.hpp"

#include <cstdint>
#include <string>

class TcpServer {
  TcpSocket socketAscultare;

public:
  TcpServer() = default;
  void Start(const std::string &ipAdress, std::uint16_t port, int backlog = 64);
  [[nodiscard]] TcpSocket Accept() const;
};