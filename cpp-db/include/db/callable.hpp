#pragma once

#include "./connection.hpp"
#include "./resultset.hpp"
#include "./statement.hpp"

namespace db {
template <typename F>
class callable {};

template <typename R, typename... A>
class callable<R(A...)> {
public:
  callable(db::connection& conn) : _stmt(conn) {
  }

  void prepare(const std::string_view script) {
    _stmt.prepare(script);
  }

  bool prepared() {
    return _stmt.prepared();
  }

  std::optional<R> operator()(A&&... a) {
    applyParams(1, a...);

    db::resultset _rslt(_stmt);
    return _rslt.firstValue<R>();
  }

private:
  db::statement _stmt;

  void applyParams(int i) {
  }

  template <typename P0, typename... P>
  void applyParams(int i, P0&& a0, P&&... a) {
    _stmt.params[std::string{":"} + std::to_string(i)] = a0;

    applyParams(i++, a...);
  }
};
} // namespace db
