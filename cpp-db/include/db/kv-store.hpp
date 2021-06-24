#pragma once

#include "./connection.hpp"
#include "./statement.hpp"

namespace db::kv {
class store {
public:
  store(db::datasource& dsrc) : _conn(dsrc), _stmt_select(_conn), _stmt_upsert(_conn) {
    _conn.execute("CREATE TABLE IF NOT EXISTS db_kv_store (key TEXT PRIMARY KEY, value TEXT)");

    _stmt_select.prepare("SELECT value FROM db_kv_store WHERE key = :key");
    _stmt_upsert.prepare("INSERT INTO db_kv_store VALUES (:key, :value) ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
  }

  db::statement::parameter& operator[](const std::string_view key) {
    _stmt_select.params[":key"] = key;
    _stmt_upsert.params[":key"] = key;
  }

private:
  db::connection _conn;

  db::statement _stmt_select;
  db::statement _stmt_upsert;
};
} // namespace db::pooled
