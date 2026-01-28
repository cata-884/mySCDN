#pragma once
#include <mutex>
#include <sqlite3.h>
#include <string>
#include <utility>
#include <vector>

typedef struct {
  std::size_t id;
  std::string node_id;
  std::string file_name;
  std::string client_ip;
  std::chrono::system_clock::time_point time;
} LogEntry;

class DatabaseManager {
  sqlite3 *conn_ptr;
  std::mutex m_mutex;
  void create_tables_if_not_exist() const;

public:
  explicit DatabaseManager(const std::string &path = "cdn.db");
  ~DatabaseManager();
  // raii
  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;

  // atribuirea fiecarui fisier catre unui nod
  void RegisterFile(const std::string &node_identificator,
                    const std::string &nume_fisier);

  // lista de {fisier, nod}
  std::vector<std::pair<std::string, std::string>> GetCatalog();

  void LogAccess(const std::string &id_nod, const std::string &file,
                 const std::string &ipAddress);

  std::vector<std::pair<std::string, int>> top_files(int limit_count);

  std::string get_owner_id(const std::string &f_name);

  std::vector<LogEntry> getLogs();

  bool CreateUser(const std::string &user, const std::string &pass,
                  const std::string &role = "user");

  std::string CheckLogin(const std::string &user, const std::string &pass);
};