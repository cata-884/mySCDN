#pragma once

#include <cstdint>
#include <expected>

  class TcpListener {
  public:
    static std::expected<int, int> create(uint16_t port, bool non_blocking = true);
  };
