#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <expected>
#include <liburing.h>
#include <span>

namespace cdn::net {

  enum class OpType : uint8_t {
    ACCEPT,
    RECV,
    SEND,
    CONNECT,
    CLOSE
  };
  struct alignas(64) IoContext{

    std::span<char> buffer;
    uint64_t connection_id{0};
    sockaddr_in client_addr;

    int fd{-1};
    int32_t result{0};

    OpType op_type;
    socklen_t addr_len{sizeof(sockaddr_in)};

  };
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

}
