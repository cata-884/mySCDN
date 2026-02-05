#include "network/TcpSocket.hpp"
#include <unistd.h>
#include <liburing.h>
namespace cdn::net {

    TcpSocket::~TcpSocket() {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }

    TcpSocket::TcpSocket(TcpSocket &&other) noexcept : m_fd(other.m_fd), m_ring(other.m_ring) {
        other.m_fd = -1;
        other.m_ring = nullptr;
    }

    TcpSocket & TcpSocket::operator=(TcpSocket &&other) noexcept {
        if (this != &other) {
            if (m_fd>=0) ::close(m_fd);
            m_ring = other.m_ring;
            m_fd = other.m_fd;
            other.m_ring = nullptr;
            other.m_fd = -1;
        }
        return *this;
    }


    TcpSocket::Result TcpSocket::submit_recv(IoContext *io_context) const {
        struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
        if (!sqe) [[unlikely]] {
            return std::unexpected(-EBUSY);
        }
        io_context->fd = m_fd;
        io_context->op_type = OpType::RECV;

        io_uring_prep_recv(sqe, m_fd, io_context->buffer.data(), io_context->buffer.size(), 0);

        io_uring_sqe_set_data(sqe, io_context);

        return {};
    }

    TcpSocket::Result TcpSocket::submit_send(IoContext *io_context) const {
        struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
        if (!sqe) [[unlikely]] {
            return std::unexpected(-EBUSY);
        }
        io_context->fd = m_fd;
        io_context->op_type = OpType::SEND;

        io_uring_prep_send(sqe, m_fd, io_context->buffer.data(), io_context->buffer.size(),
            MSG_NOSIGNAL);

        io_uring_sqe_set_data(sqe, io_context);

        return {};
    }

    TcpSocket::Result TcpSocket::submit_accept(IoContext *io_context) const {
        struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
        if (!sqe) [[unlikely]] {
            return std::unexpected(-EBUSY);
        }
        io_context->fd = m_fd;
        io_context->op_type = OpType::ACCEPT;

        io_uring_prep_accept(sqe, m_fd, reinterpret_cast<sockaddr *>(&io_context->client_addr), &io_context->addr_len,0);

        io_uring_sqe_set_data(sqe, io_context);

        return {};
    }
}
