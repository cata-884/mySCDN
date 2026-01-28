#pragma once

#include <cstdint>
#include <string>
#include <utility>

struct PeerDescriptor {
  std::string ID;
  std::string ipAdress;
  std::uint16_t port;
  PeerDescriptor() : port(0) {}
  PeerDescriptor(std::string id, std::string ip, const std::uint16_t p)
      : ID(std::move(id)), ipAdress(std::move(ip)), port(p) {}
};
