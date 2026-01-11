#pragma once
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

inline void throwIF(bool condition, const std::string &msg) {
  if (condition) {
    throw std::runtime_error(msg + ": " + std::strerror(errno));
  }
}