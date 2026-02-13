#pragma once
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

inline void throwIF(const bool condition, std::string_view msg) {
  if (condition) {
    std::string full(msg);
    full += ": ";
    full += std::strerror(errno);
    throw std::runtime_error(full);
  }
}