#pragma once

#include "./connection.hpp"
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>

namespace db {
class statement {
public:
  friend resultset;

  class parameter {
  public:
    explicit parameter(statement& stmt, const std::string_view name) : _stmt(stmt), _name(name) {
    }

    parameter(const parameter&) = delete;

    parameter(parameter&&) = default;

    parameter& operator=(const parameter&) = delete;

    parameter& operator=(parameter&&) = default;

    inline parameter& operator=(bool value) {
      setValue(value);
      return *this;
    }

    inline parameter& operator=(int value) {
      setValue(value);
      return *this;
    }

    inline parameter& operator=(int64_t value) {
      setValue(value);
      return *this;
    }

#ifdef __SIZEOF_INT128__
    inline parameter& operator=(__uint128_t value) {
      setValue(value);
      return *this;
    }
#endif

    inline parameter& operator=(double value) {
      setValue(value);
      return *this;
    }

    inline parameter& operator=(const std::string_view value) {
      setValue(value);
      return *this;
    }

    inline parameter& operator=(const char* value) {
      return (*this) = std::string_view{value};
    }

    inline parameter& operator=(const std::vector<uint8_t>& value) {
      setValue(value);
      return *this;
    }

    template <typename T>
    inline parameter& operator=(const std::optional<T>& value) {
      setValueByOptional<T>(value);
      return *this;
    }

    inline parameter& operator=(orm::date value) {
      setValue(value);
      return *this;
    }

    inline parameter& operator=(orm::time value) {
      setValue(value);
      return *this;
    }

    inline parameter& operator=(orm::datetime value) {
      setValue(value);
      return *this;
    }

    inline parameter& operator=(std::nullopt_t) {
      setNull();
      return *this;
    }

  private:
    std::reference_wrapper<statement> _stmt;
    std::string _name;

    template <typename T>
    void setValueByOptional(const std::optional<T>& value) {
      if (value) {
        if constexpr (type_converter<T>::specialized) {
          setValue(type_converter<T>::serialize(*value));
        } else {
          setValue(*value);
        }
      } else {
        setNull();
      }
    }

    template <typename T>
    void setValue(const T& value) {
      _stmt.get().assertPrepared();
      _stmt.get()._datasource_statement->setParam(_name, value);
    }

    void setNull() {
      _stmt.get().assertPrepared();
      _stmt.get()._datasource_statement->setParamToNull(_name);
    }
  };

  class parameter_access {
  public:
    explicit parameter_access(statement& stmt) : _stmt(stmt) {
    }

    parameter& operator[](const std::string_view name) {
      return *(_current_parameter = statement::parameter(_stmt, name));
    }

  private:
    std::reference_wrapper<statement> _stmt;
    std::optional<parameter> _current_parameter;
  };

  parameter_access params = parameter_access(*this);

  explicit statement(connection& conn) : _conn(conn) {
  }

  statement(const statement&) = delete;

  statement(statement&&) = delete;

  statement& operator=(const statement&) = delete;

  statement& operator=(statement&&) = delete;

  void prepare(const std::string_view script) {
    _datasource_statement = _conn.get()._datasource_connection->prepareStatement(script);
  }

  bool prepared() {
    return !!_datasource_statement;
  }

  void assertPrepared() {
    if (!prepared()) {
      // throw new std::runtime_error("statement not yet prepared");
    }
  }

  int executeUpdate() {
    assertPrepared();

    return _datasource_statement->executeUpdate();
  }

  template <typename T = datasource::statement>
  std::shared_ptr<T> getNativeStatement() {
    return std::dynamic_pointer_cast<T>(_datasource_statement);
  }

protected:
  std::reference_wrapper<connection> _conn;
  std::shared_ptr<datasource::statement> _datasource_statement;
};
} // namespace db
