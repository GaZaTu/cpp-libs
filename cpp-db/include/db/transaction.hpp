#pragma once

#include "./connection.hpp"
#include <memory>

namespace db {
class transaction {
public:
  explicit transaction(connection& conn) : _conn(conn) {
    _conn.get()._datasource_connection->beginTransaction();
  }

  transaction(const transaction&) = delete;

  transaction(transaction&&) = delete;

  transaction& operator=(const transaction&) = delete;

  transaction& operator=(transaction&&) = delete;

  ~transaction() {
    if (!_didCommitOrRollback) {
      if (std::uncaught_exceptions() == 0) {
        commit();
      } else {
        rollback();
      }
    }
  }

  void commit() {
    _conn.get()._datasource_connection->commit();
    _didCommitOrRollback = true;
  }

  void rollback() {
    _conn.get()._datasource_connection->rollback();
    _didCommitOrRollback = true;
  }

private:
  std::reference_wrapper<connection> _conn;
  bool _didCommitOrRollback = false;
};
} // namespace db
