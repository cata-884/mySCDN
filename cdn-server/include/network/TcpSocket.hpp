#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class TcpSocket {
  int sockFD{-1};
  std::string ip;
  std::uint16_t port{0};

  std::vector<char> recvBuf_;
  std::size_t bufPos_{0};
  std::size_t bufLen_{0};

  ssize_t fillBuffer();

  explicit TcpSocket(int _sockFD, std::string _ip, std::uint16_t _port);

public:
  TcpSocket();
  ~TcpSocket();

  TcpSocket(const TcpSocket &sockFD) = delete;
  TcpSocket &operator=(const TcpSocket &) = delete;
  TcpSocket(TcpSocket &&other) noexcept;
  TcpSocket &operator=(TcpSocket &&other) noexcept;

  void Bind(std::uint16_t _port, const std::string &ipAdress = "0.0.0.0") const;
  void Listen(int backlog = 64) const;
  [[nodiscard]] TcpSocket Accept() const;
  void Connect(std::uint16_t port, const std::string &ipAdress);
  void Close();
  void SendAll(std::string_view mesaj) const;
  void SendAll(const void *mesaj, std::size_t len) const;
  bool SendFile(int fileFd, std::size_t fileSize) const;
  std::string recvLine(std::size_t maxLen = 4096);
  [[nodiscard]] std::string recvN(std::size_t len);
  [[nodiscard]] bool isValid() const;
  [[nodiscard]] int getSockFD() const { return sockFD; }
  std::size_t Recv(void *buffer, std::size_t len);
  std::size_t Recv(std::span<char> buffer);
  [[nodiscard]] std::string getIP() const { return ip; }
  [[nodiscard]] std::uint16_t getPort() const { return port; }
};
