#pragma once

#include <sys/socket.h>
#include <expected>
#include <liburing.h>
#include <span>

#include "utils/NetworkTypes.hpp"

class TcpSocket {
  public:
    using Result = std::expected<void, int>;

    TcpSocket(const int fd, io_uring* ring) : m_fd(fd), m_ring(ring) {}
    ~TcpSocket();

    TcpSocket(const TcpSocket &m_fd) = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;

    TcpSocket(TcpSocket &&other) noexcept;
    TcpSocket &operator=(TcpSocket &&other) noexcept;

    [[nodiscard]] int fd() const {return m_fd;}
    Result submit_recv(IoContext* io_context) const;
    Result submit_send(IoContext* io_context) const;
    Result submit_accept(IoContext* io_context) const;

  private:
    int m_fd{-1};
    io_uring* m_ring{nullptr};
  };

