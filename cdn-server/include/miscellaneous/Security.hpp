#pragma once
#include <iomanip>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <sstream>
#include <string>

class SecurityUtils {
public:
  static std::string GenerateSalt(const size_t length = 16) {
    const auto buffer = new unsigned char[length];
    if (RAND_bytes(buffer, static_cast<int>(length)) != 1) {
      for (size_t i = 0; i < length; ++i)
        buffer[i] = static_cast<unsigned char>(rand() % 255);
    }
    std::stringstream ss;
    for (size_t i = 0; i < length; ++i)
      ss << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<int>(buffer[i]);
    delete[] buffer;
    return ss.str();
  }

  static std::string HashPassword(const std::string &password,
                                  const std::string &salt) {
    const std::string combined = password + salt;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(combined.c_str()),
           combined.size(), hash);
    std::stringstream ss;
    for (const unsigned char i : hash)
      ss << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<int>(i);
    return ss.str();
  }
};