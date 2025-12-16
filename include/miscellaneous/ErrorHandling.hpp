#pragma once
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <cerrno>


inline void throwIF(bool condition, const std::string& msg) {
    if (condition) {
        throw std::runtime_error(msg + ": " + std::strerror(errno));
    }
}