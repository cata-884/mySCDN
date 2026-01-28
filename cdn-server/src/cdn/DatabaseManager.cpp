#include "cdn/Database.hpp"
#include "miscellaneous/ErrorHandling.hpp"
#include "miscellaneous/Security.hpp"
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sqlite3.h>

/*possible values for rc = {
    SQLITE_OK 0 - success
    SQLITE_ERROR 1 - generic error
    SQLITE_CANTOPEN 14 - nu pot deschide .db
    SQLITE_BUSY 5 - baza de date e blocat de alt thread
    SQLITE_MISUSE 21 - am apelat functia gresit
}
*/

DatabaseManager::DatabaseManager(const std::string &path) : conn_ptr(nullptr) {
  // return code
  const int ret_val = sqlite3_open(path.c_str(), &conn_ptr);

  throwIF(ret_val != SQLITE_OK,
          std::string("[DB ERROR] Nu pot deschide baza de date: ") +
              sqlite3_errmsg(conn_ptr) + "\n");
  create_tables_if_not_exist();
}

DatabaseManager::~DatabaseManager() {
  if (conn_ptr) {
    sqlite3_close(conn_ptr);
    std::cout << "[DB] Conexiune inchisa\n";
  }
}

void DatabaseManager::create_tables_if_not_exist() const {
  // access_logs = {id, node_id, file_name, client_ip, timestamp}
  const auto str_sql_logs =
      "CREATE TABLE IF NOT EXISTS access_logs (id INTEGER PRIMARY KEY "
      "AUTOINCREMENT, node_id TEXT NOT NULL, file_name TEXT NOT NULL, "
      "client_ip TEXT NOT NULL, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
  // catalog = {file_name, node_id}
  const auto str_sql_catalog =
      "CREATE TABLE IF NOT EXISTS file_catalog (file_name TEXT PRIMARY KEY, "
      "node_id TEXT NOT NULL);";

  const auto str_sql_users = "CREATE TABLE IF NOT EXISTS users ("
                              "username TEXT PRIMARY KEY, "
                              "password_hash TEXT NOT NULL, "
                              "salt TEXT NOT NULL, "
                              "role TEXT NOT NULL);";

  char *errMsg = nullptr;
  sqlite3_exec(conn_ptr, str_sql_users, nullptr, nullptr, &errMsg);
  if (errMsg) {
    std::cerr << "[DB ERROR] Nu s-a putut crea tabela users: " << errMsg
              << "\n";
    sqlite3_free(errMsg);
  }

  sqlite3_exec(conn_ptr, str_sql_logs, nullptr, nullptr, &errMsg);
  if (errMsg) {
    std::cerr << "[DB ERROR] Nu s-a putut crea access_logs: " << errMsg << "\n";
    sqlite3_free(errMsg);
  }
  sqlite3_exec(conn_ptr, str_sql_catalog, nullptr, nullptr, &errMsg);
  if (errMsg) {
    std::cerr << "[DB ERROR] Nu s-a putut crea file_catalog: " << errMsg
              << "\n";
    sqlite3_free(errMsg);
  }
}

void DatabaseManager::LogAccess(const std::string &id_nod,
                                const std::string &file,
                                const std::string &ipAddress) {
  if (!conn_ptr)
    return;
  std::lock_guard guard(m_mutex);

  sqlite3_stmt *prepared_stmt;
  const auto query = "INSERT INTO access_logs (node_id, file_name, client_ip) "
                      "VALUES (?, ?, ?);";
  if (sqlite3_prepare_v2(conn_ptr, query, -1, &prepared_stmt, nullptr) !=
      SQLITE_OK)
    return;
  // SQLITE_TRANSIENT-folosim cand string-ul dat este mutabil
  // SQLITE_STATIC - const
  sqlite3_bind_text(prepared_stmt, 1, id_nod.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(prepared_stmt, 2, file.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(prepared_stmt, 3, ipAddress.c_str(), -1, SQLITE_STATIC);
  if (sqlite3_step(prepared_stmt) != SQLITE_DONE) {
    std::cerr << "[DB ERROR] Insert a esuat: " << sqlite3_errmsg(conn_ptr)
              << std::endl;
  }
  sqlite3_finalize(prepared_stmt);
}

void DatabaseManager::RegisterFile(const std::string &node_identificator,
                                   const std::string &nume_fisier) {
  if (!conn_ptr)
    return;
  std::lock_guard guard(m_mutex);

  sqlite3_stmt *stmt_insert;
  const auto sql_command =
      "INSERT OR REPLACE INTO file_catalog (file_name, node_id) VALUES (?, ?);";
  if (sqlite3_prepare_v2(conn_ptr, sql_command, -1, &stmt_insert, nullptr) !=
      SQLITE_OK)
    return;

  sqlite3_bind_text(stmt_insert, 1, nume_fisier.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt_insert, 2, node_identificator.c_str(), -1,
                    SQLITE_STATIC);
  if (sqlite3_step(stmt_insert) != SQLITE_DONE) {
    std::cerr << "[DB ERROR] Insert failed: " << sqlite3_errmsg(conn_ptr)
              << std::endl;
  }
  sqlite3_finalize(stmt_insert);
}

std::vector<std::pair<std::string, std::string>> DatabaseManager::GetCatalog() {
  std::lock_guard lk(m_mutex);
  std::vector<std::pair<std::string, std::string>> result_vector;
  if (!conn_ptr)
    return result_vector;

  const std::string sql_select =
      "SELECT file_name, node_id FROM file_catalog ORDER BY file_name ASC;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(conn_ptr, sql_select.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK)
    return result_vector;
  // sqlite3_step(stmt) executa comanda si returneaza un rand
  // exec - prepare->step->finalize, bun pentru comenzi statice
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    auto fisier = std::string(
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
    auto nod_sursa = std::string(
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
    result_vector.emplace_back(fisier, nod_sursa);
  }
  sqlite3_finalize(stmt);
  return result_vector;
}

std::vector<std::pair<std::string, int>>
DatabaseManager::top_files(const int limit_count) {
  std::lock_guard lock(m_mutex);
  std::vector<std::pair<std::string, int>> output_list;
  if (!conn_ptr)
    return output_list;
  // de cate ori apare fiecare fisier in log-uri
  const std::string complex_q =
      "SELECT file_name, COUNT(*) as hit_count FROM access_logs "
      "GROUP BY file_name "
      "ORDER BY hit_count DESC "
      "LIMIT ?;";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(conn_ptr, complex_q.c_str(), -1, &s, nullptr) !=
      SQLITE_OK) {
    std::cerr << "[DB ERROR] GetTopFiles prepare a esuat: "
              << sqlite3_errmsg(conn_ptr) << std::endl;
    return output_list;
  }
  sqlite3_bind_int(s, 1, limit_count);

  while (sqlite3_step(s) == SQLITE_ROW) {
    const unsigned char *text_val = sqlite3_column_text(s, 0);
    int count_hits = sqlite3_column_int(s, 1);
    if (text_val) {
      output_list.emplace_back(
          std::string(reinterpret_cast<const char *>(text_val)), count_hits);
    }
  }
  sqlite3_finalize(s);
  return output_list;
}

std::string DatabaseManager::get_owner_id(const std::string &f_name) {
  std::lock_guard lock(m_mutex);
  if (!conn_ptr)
    return "";
  const std::string q = "SELECT node_id FROM file_catalog WHERE file_name = ?;";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(conn_ptr, q.c_str(), -1, &st, nullptr) != SQLITE_OK)
    return "";
  sqlite3_bind_text(st, 1, f_name.c_str(), -1, SQLITE_STATIC);
  std::string owner;
  if (sqlite3_step(st) == SQLITE_ROW) {
    owner =
        std::string(reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
  }
  sqlite3_finalize(st);
  return owner;
}
// access_logs = {id, node_id, file_name, client_ip, timestamp}

std::vector<LogEntry> DatabaseManager::getLogs() {
  std::lock_guard lock(m_mutex);
  std::vector<LogEntry> res;
  if (!conn_ptr)
    return res;

  const std::string q = "SELECT id, node_id, file_name, client_ip, strftime('%s', "
                  "timestamp) FROM access_logs ORDER BY id ASC;";

  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(conn_ptr, q.c_str(), -1, &st, nullptr) != SQLITE_OK)
    return res;

  while (sqlite3_step(st) == SQLITE_ROW) {
    LogEntry entry;
    entry.id = static_cast<std::size_t>(sqlite3_column_int64(st, 0));
    entry.node_id =
        std::string(reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
    entry.file_name =
        std::string(reinterpret_cast<const char *>(sqlite3_column_text(st, 2)));
    entry.client_ip =
        std::string(reinterpret_cast<const char *>(sqlite3_column_text(st, 3)));
    const long long secunde = sqlite3_column_int64(st, 4);
    entry.time = std::chrono::system_clock::from_time_t(
        secunde);
    res.emplace_back(entry);
  }
  sqlite3_finalize(st);
  return res;
}

bool DatabaseManager::CreateUser(const std::string &user,
                                 const std::string &pass,
                                 const std::string &role) {
  if (!conn_ptr)
    return false;
  std::lock_guard guard(m_mutex);

  const std::string salt = SecurityUtils::GenerateSalt();
  const std::string hash = SecurityUtils::HashPassword(pass, salt);

  sqlite3_stmt *stmt;
  const auto sql = "INSERT INTO users (username, password_hash, salt, role) "
                    "VALUES (?, ?, ?, ?);";

  if (sqlite3_prepare_v2(conn_ptr, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, role.c_str(), -1, SQLITE_STATIC);

  bool success = false;
  if (sqlite3_step(stmt) == SQLITE_DONE) {
    success = true;
  } else {
    std::cerr << "[DB] Register failed (user taken?): "
              << sqlite3_errmsg(conn_ptr) << std::endl;
  }
  sqlite3_finalize(stmt);
  return success;
}

std::string DatabaseManager::CheckLogin(const std::string &user,
                                        const std::string &pass) {
  std::lock_guard guard(m_mutex);
  if (!conn_ptr)
    return "";

  const auto sql =
      "SELECT password_hash, salt, role FROM users WHERE username = ?;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(conn_ptr, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return "";

  sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);

  std::string found_role;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const std::string db_hash =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    const std::string db_salt =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    const std::string db_role =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));

    // Verificam hash-ul
    if (SecurityUtils::HashPassword(pass, db_salt) == db_hash) {
      found_role = db_role;
    }
  }
  sqlite3_finalize(stmt);
  return found_role;
}