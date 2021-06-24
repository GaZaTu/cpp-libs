#pragma once

#include <stdexcept>

namespace db {
class resultset;
class statement;
class transaction;
class connection;

class sql_error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

template <typename T>
struct type_converter {
  static constexpr bool specialized = false;
};
} // namespace db
