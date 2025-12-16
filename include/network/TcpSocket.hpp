#pragma once

#include <cstddef>
#include <cstdint>
#include <string>


class TcpSocket{
    int sockFD = -1;
    explicit TcpSocket(int sockFD);
public:
    //clasic oop
    TcpSocket();
    ~TcpSocket();
    //RAII
    TcpSocket(const TcpSocket& sockFD) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;
    //abstractizari
    void Bind(std::uint16_t port, const std::string& ipAdress = "0.0.0.0");
    void Listen(int backlog = 64) const;
    TcpSocket Accept() const;
    void Connect(std::uint16_t port, const std::string& ipAdress);
    void Close();
    void SendAll(const std::string& mesaj) const;
    void SendAll(const void* mesaj, std::size_t len) const;
    std::string recvLine(std::size_t len = 4096) const;
    std::string recvN(std::size_t len) const;
    bool isValid() const;
    int getSockFD() const { return sockFD; }
    std::size_t Recv(void* buffer, std::size_t len) const;
};
