#include "network/TcpListener.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

    std::expected<int, int> TcpListener::create(const uint16_t port, const bool non_blocking) {

        const int fd = ::socket(AF_INET6, SOCK_STREAM, 0);
        if (fd < 0) return std::unexpected(errno);
        constexpr int opt = 1;

        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            ::close(fd);
            return std::unexpected(errno);
        }

        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            ::close(fd);
            return std::unexpected(errno);
        }
        //ipv6 willing to accept ipv4
        constexpr int no = 0;
        if (::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) < 0) {
            ::close(fd);
            return std::unexpected(errno);
        }

        if (non_blocking) {
            //get actual flags
            const int flags = ::fcntl(fd, F_GETFL, 0);
            /*
             Add nonblock flag - if there's not one client => accept(...) < 0 and (errno == EAGAIN or EWOULDBLOCK).
                 Through this, the thread never freezes and can process other events through io_uring.
            */
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        addr.sin6_addr = in6addr_any;

        if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            return std::unexpected(errno);
        }
        if (::listen(fd, SOMAXCONN) < 0) {
            ::close(fd);
            return std::unexpected(errno);
        }
        return fd;

    }
