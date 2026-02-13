#include "network/TcpSocket.hpp"
#include "miscellaneous/ErrorHandling.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <span>
#include <string>
#include <string_view>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static constexpr std::size_t kRecvBufSize = 8192;

static void setTcpNoDelay(int fd) {
  constexpr int flag = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

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

TcpSocket::TcpSocket()
    : sockFD(openSocket()), recvBuf_(kRecvBufSize), bufPos_(0), bufLen_(0) {}

TcpSocket::TcpSocket(const int _sockFD, std::string _ip,
                     const std::uint16_t _port)
    : sockFD(_sockFD), ip(std::move(_ip)), port(_port), recvBuf_(kRecvBufSize),
      bufPos_(0), bufLen_(0) {
  setTcpNoDelay(_sockFD);
}

TcpSocket::~TcpSocket() { Close(); }

TcpSocket::TcpSocket(TcpSocket &&other) noexcept
    : sockFD(other.sockFD), ip(std::move(other.ip)), port(other.port),
      recvBuf_(std::move(other.recvBuf_)), bufPos_(other.bufPos_),
      bufLen_(other.bufLen_) {
  other.sockFD = -1;
  other.port = 0;
  other.bufPos_ = 0;
  other.bufLen_ = 0;
}

TcpSocket &TcpSocket::operator=(TcpSocket &&other) noexcept {
  if (this != &other) {
    Close();
    sockFD = other.sockFD;
    other.sockFD = -1;
    ip = std::move(other.ip);
    port = other.port;
    other.port = 0;
    recvBuf_ = std::move(other.recvBuf_);
    bufPos_ = other.bufPos_;
    bufLen_ = other.bufLen_;
    other.bufPos_ = 0;
    other.bufLen_ = 0;
  }
  return *this;
}

ssize_t TcpSocket::fillBuffer() {
  bufPos_ = 0;
  const ssize_t n = ::recv(sockFD, recvBuf_.data(), recvBuf_.size(), 0);
  bufLen_ = (n > 0) ? static_cast<std::size_t>(n) : 0;
  return n;
}

void TcpSocket::Bind(const std::uint16_t _port,
                     const std::string &ipAdress) const {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(_port);
  if (ipAdress.empty() || ipAdress == "0.0.0.0" || ipAdress == "0") {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    const int ipSetStatus =
        ::inet_pton(AF_INET, ipAdress.c_str(), &addr.sin_addr);
    throwIF(ipSetStatus <= 0, "Adresa IP invalida:" + ipAdress);
  }
  const int bind_status =
      ::bind(sockFD, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
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
  bufPos_ = 0;
  bufLen_ = 0;
}

void TcpSocket::Connect(const std::uint16_t _port,
                        const std::string &ipAdress) {
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
  setTcpNoDelay(sockFD);
}

void TcpSocket::SendAll(std::string_view mesaj) const {
  SendAll(mesaj.data(), mesaj.size());
}

void TcpSocket::SendAll(const void *mesaj, const std::size_t len) const {
  const auto buff = static_cast<const char *>(mesaj);
  std::size_t total = 0;

  while (total < len) {
    const ssize_t trimis =
        ::send(sockFD, buff + total, len - total, MSG_NOSIGNAL);
    if (trimis < 0 && errno == EINTR)
      continue;
    throwIF(trimis <= 0, "Eroare la send");
    total += static_cast<std::size_t>(trimis);
  }
}

std::string TcpSocket::recvLine(const std::size_t maxLen) {
  std::string linie;
  linie.reserve(128);

  while (linie.size() < maxLen) {
    if (bufPos_ >= bufLen_) {
      const ssize_t n = fillBuffer();
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        break;
    }

    const char *start = recvBuf_.data() + bufPos_;
    const std::size_t avail = bufLen_ - bufPos_;

    const char *nl = static_cast<const char *>(std::memchr(start, '\n', avail));

    if (nl) {
      const std::size_t chunk = static_cast<std::size_t>(nl - start);
      linie.append(start, chunk);
      bufPos_ += chunk + 1;

      if (!linie.empty() && linie.back() == '\r')
        linie.pop_back();
      return linie;
    }

    linie.append(start, avail);
    bufPos_ = bufLen_;
  }

  if (!linie.empty() && linie.back() == '\r')
    linie.pop_back();
  return linie;
}

std::string TcpSocket::recvN(const std::size_t len) {
  std::string data(len, '\0');
  std::size_t totalPrimit = 0;

  const std::size_t buffered = bufLen_ - bufPos_;
  if (buffered > 0) {
    const std::size_t toCopy = (buffered < len) ? buffered : len;
    std::memcpy(&data[0], recvBuf_.data() + bufPos_, toCopy);
    bufPos_ += toCopy;
    totalPrimit = toCopy;
  }

  while (totalPrimit < len) {
    const ssize_t partialPrimit =
        ::recv(sockFD, &data[0] + totalPrimit, len - totalPrimit, 0);
    if (partialPrimit < 0 && errno == EINTR)
      continue;
    throwIF(partialPrimit <= 0,
            "Eroare la citire continutului din fisier(payload)");
    totalPrimit += static_cast<std::size_t>(partialPrimit);
  }
  return data;
}

bool TcpSocket::isValid() const { return sockFD >= 0; }

std::size_t TcpSocket::Recv(void *buffer, const std::size_t len) {
  auto dest = static_cast<char *>(buffer);
  std::size_t total = 0;

  const std::size_t buffered = bufLen_ - bufPos_;
  if (buffered > 0) {
    const std::size_t toCopy = (buffered < len) ? buffered : len;
    std::memcpy(dest, recvBuf_.data() + bufPos_, toCopy);
    bufPos_ += toCopy;
    total = toCopy;
  }

  if (total < len) {
    const ssize_t primit = ::recv(sockFD, dest + total, len - total, 0);
    if (primit > 0)
      total += static_cast<std::size_t>(primit);
  }

  return total;
}

std::size_t TcpSocket::Recv(std::span<char> buffer) {
  return Recv(buffer.data(), buffer.size());
}

bool TcpSocket::SendFile(int fileFd, std::size_t fileSize) const {
  off_t offset = 0;
  std::size_t remaining = fileSize;
  while (remaining > 0) {
    const ssize_t sent = ::sendfile(sockFD, fileFd, &offset, remaining);
    if (sent < 0) {
      if (errno == EINTR || errno == EAGAIN)
        continue;
      return false;
    }
    if (sent == 0)
      break;
    remaining -= static_cast<std::size_t>(sent);
  }
  return remaining == 0;
}