#pragma once

#include <cstdint>
#include <string>

struct PeerDescriptor {
    std::string ID;
    std::string ipAdress;
    std::uint16_t port;
    PeerDescriptor() : port(0) {}
    PeerDescriptor(std::string id, std::string ip, std::uint16_t p) : ID(id), ipAdress(ip), port(p) {} 
};
