#pragma once

#include <cstdint>
#include <expected>

namespace cdn::net {
  class TcpListener {
  public:
    static std::expected<int, int> create(uint16_t port, bool non_blocking = true);
  };
}