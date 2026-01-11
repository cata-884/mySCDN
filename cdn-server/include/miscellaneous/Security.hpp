#pragma once
#include <iomanip>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <sstream>
#include <string>

class SecurityUtils {
public:
  static std::string GenerateSalt(size_t length = 16) {
    unsigned char *buffer = new unsigned char[length];
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
    std::string combined = password + salt;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(combined.c_str()),
           combined.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
      ss << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<int>(hash[i]);
    return ss.str();
  }
};