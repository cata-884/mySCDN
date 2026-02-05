#pragma once
#include<liburing.h>
#include<filesystem>
#include<expected>
#include<utility>
#include"utils/NetworkTypes.hpp"

class ReadOnlyFile {
    public:
    using Result = std::expected<void, int>;

    ReadOnlyFile(const int fd, const size_t size, std::filesystem::path path) :
        m_fd(fd), m_size(size), m_path(std::move(path)){}

    ~ReadOnlyFile() {
        ::close(m_fd);
    }

    ReadOnlyFile(const ReadOnlyFile&) = delete;
    ReadOnlyFile& operator=(const ReadOnlyFile&) = delete;

    ReadOnlyFile(ReadOnlyFile&& other) noexcept : m_fd(std::exchange(other.m_fd, -1)),
                                                  m_size(std::exchange(other.size(), 0)),
                                                  m_path(std::move(other.m_path)) {}

    ReadOnlyFile& operator=(ReadOnlyFile&& other) {
        if (this != &other) {
            if (m_fd >=0) ::close(m_fd);
            m_fd = std::exchange(other.m_fd, -1);
            m_size = std::exchange(other.m_size, 0);
            m_path = std::move(other.m_path);
        }
        return *this;
    }
    Result submit_read(io_uring* ring, IoContext* ctx, const uint64_t offset, size_t len) const {
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe) [[unlikely]] {
            return std::unexpected(-EBUSY);
        }
        ctx->m_fd = m_fd;
        ctx->op_type = OpType::READ;
        const std::span<char> buff = ctx->get_buffer_view();
        const size_t to_read = std::min(len, buff.size());
        io_uring_prep_read(sqe, m_fd, buff.data(), to_read, offset);
        io_uring_sqe_set_data(sqe, buff.data());
        return {};
    }
    [[nodiscard]] size_t size() const { return m_size; }
    [[nodiscard]] int fd() const { return m_fd; }
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }
private:
    int m_fd{-1};
    ssize_t m_size{0};
    std::filesystem::path m_path{};
};