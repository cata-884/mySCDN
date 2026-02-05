#pragma once

#include <sys/socket.h>
#include <liburing.h>
#include <span>

  enum class OpType : uint8_t {
      ACCEPT,
      RECV,
      SEND,
      CONNECT,
      CLOSE,
      READ,
      OTHER
    };
struct alignas(64) IoContext{

    static constexpr size_t BUFFER_SIZE = 4096;
    std::array<char, BUFFER_SIZE> m_buffer_storage{};
    sockaddr_storage m_client_addr{};
    socklen_t m_addr_len{sizeof(sockaddr_storage)};
    uint64_t m_connection_id{0};
    int m_fd{-1};
    int32_t m_result{0};

    OpType op_type{OpType::OTHER};

    std::span<char> get_buffer_view() {
        return {m_buffer_storage.data(), BUFFER_SIZE};
    }

    void reset() {
        m_addr_len = sizeof(sockaddr_storage);
        m_fd = -1;
        m_result = 0;
    }

};