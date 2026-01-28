#include "network/TcpSocket.hpp"
#include "miscellaneous/ErrorHandling.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int openSocket() {
  const int sockFD = ::socket(AF_INET, SOCK_STREAM, 0);
  throwIF(sockFD < 0, "S-a esuat crearea unui socket");
  constexpr int opt = 1;
  const int sockOptResult =
      ::setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (sockOptResult < 0) {
    ::close(sockFD);
    throwIF(true, "S-a esuat refolosirea aceluias IP");
  }
  return sockFD;
}
TcpSocket::TcpSocket() : sockFD(openSocket()) {}
TcpSocket::TcpSocket(const int sockFD, std::string ip, const std::uint16_t port)
    : sockFD(sockFD), ip(std::move(ip)), port(port) {}
TcpSocket::~TcpSocket() { Close(); }

TcpSocket::TcpSocket(TcpSocket &&other) noexcept
    : sockFD(other.sockFD), ip(std::move(other.ip)), port(other.port) {
  other.sockFD = -1;
  other.port = 0;
}

TcpSocket &TcpSocket::operator=(TcpSocket &&other) noexcept {
  if (this != &other) {
    Close();
    sockFD = other.sockFD;
    other.sockFD = -1;
    ip = std::move(other.ip);
    port = other.port;
    other.port = 0;
  }
  return *this;
}

void TcpSocket::Bind(const std::uint16_t _port, const std::string &ipAdress) const {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(_port);
  if (ipAdress.empty() || ipAdress == "0.0.0.0" || ipAdress == "0") {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    const int ipSetStatus = ::inet_pton(AF_INET, ipAdress.c_str(), &addr.sin_addr);
    throwIF(ipSetStatus <= 0, "Adresa IP invalida:" + ipAdress);
  }
  const int bind_status =
      ::bind(sockFD, /*(sockaddr*)&addr*/ reinterpret_cast<sockaddr *>(&addr),
             sizeof(addr));
  throwIF(bind_status < 0, "Eroare la bind folosind IP-ul " +
                            (ipAdress.empty() ? "ANY" : ipAdress) +
                            " si portul " + std::to_string(port));
}

void TcpSocket::Listen(const int backlog) const {
  throwIF(::listen(sockFD, backlog) < 0, "Eroare la Listen");
}

TcpSocket TcpSocket::Accept() const {
  sockaddr_in clientAddr{};
  socklen_t len = sizeof(clientAddr);
  int clientFD =
      ::accept(sockFD, reinterpret_cast<sockaddr *>(&clientAddr), &len);
  // errno == EAGAIN (Resource Temporarily Unavailable)
  // errno == EINTR (Interrupted System Call)
  while (clientFD < 0 && (errno == EINTR || errno == EAGAIN)) {
    clientFD =
        ::accept(sockFD, reinterpret_cast<sockaddr *>(&clientAddr), &len);
  }

  throwIF(clientFD < 0, "Eroare la Accept");

  char buffer[INET_ADDRSTRLEN];
  ::inet_ntop(AF_INET, &clientAddr.sin_addr, buffer, sizeof(buffer));

  const std::uint16_t _port = ntohs(clientAddr.sin_port);
  return TcpSocket(clientFD, std::string(buffer), _port);
}

void TcpSocket::Close() {
  if (sockFD >= 0) {
    ::close(sockFD);
    sockFD = -1;
  }
}

void TcpSocket::Connect(const std::uint16_t _port, const std::string &ipAdress) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(_port);

  throwIF(::inet_pton(AF_INET, ipAdress.c_str(), &addr.sin_addr) <= 0,
          "Adresa IP invalida la connect: " + ipAdress);

  const int res =
      ::connect(sockFD, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
  throwIF(res < 0, "Nu s-a putut realiza conexiunea la server");
  this->ip = ipAdress;
  this->port = _port;
}

void TcpSocket::SendAll(const std::string &mesaj) const {
  SendAll(mesaj.data(), mesaj.size());
}

void TcpSocket::SendAll(const void *mesaj, const std::size_t len) const {
  const auto buff = static_cast<const char *>(mesaj);
  std::size_t total = 0;
  // trimitem pe chunks
  while (total < len) {
    const auto trimis =
        static_cast<std::size_t>(::send(sockFD, buff + total, len - total, 0));
    throwIF(trimis <= 0, "Eroare la send");
    total += trimis;
  }
}

std::string TcpSocket::recvLine(const std::size_t len) const {
  std::string linie;
  linie.reserve(64);
  while (linie.size() < len) {
    char c;

    const ssize_t primit = ::recv(sockFD, &c, 1, 0);
    if (primit < 0 && errno == EINTR)
      continue;
    throwIF(primit <= 0, "Conexiunea s-a incheiat in timp ce se citea linia");
    if (c == '\n') {
      break;
    }
    // windows-ul transmite si \r\n
    if (c != '\r') {
      linie.push_back(c);
    }
  }
  return linie;
}

std::string TcpSocket::recvN(const std::size_t len) const {
  std::string data(len, 0);
  std::size_t totalPrimit = 0;
  while (totalPrimit < len) {
    const ssize_t partialPrimit =
        ::recv(sockFD, &data[0] + totalPrimit, len - totalPrimit, 0);
    throwIF(partialPrimit <= 0,
            "Eroare la citire continutului din fisier(payload)");
    totalPrimit += static_cast<std::size_t>(partialPrimit);
  }
  return data;
}
bool TcpSocket::isValid() const { return sockFD >= 0; }

std::size_t TcpSocket::Recv(void *buffer, const std::size_t len) const {
  const ssize_t primit = ::recv(sockFD, buffer, len, 0);
  return static_cast<std::size_t>(primit);
}