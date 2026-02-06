#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "rocksdb/db.h"

// Structura care stă în DB ca valoare pentru un fișier
struct FileMetadata {
  std::string physical_hash;  // Hash-ul pe disc (ex: sha256)
  size_t size;                // Dimensiunea în bytes
  std::string content_type;   // ex: "video/mp4"
};

class DatabaseManager {
public:

  std::shared_ptr<rocksdb::DB> getRawDB() const { return m_db; }

private:
  std::shared_ptr<rocksdb::DB> m_db;
};