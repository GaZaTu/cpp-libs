#pragma once

#include "./common.hpp"
#include "./orm-common.hpp"
#include <functional>
#include <memory>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace db {
class datasource {
public:
  class resultset {
  public:
    virtual ~resultset() {
    }

    virtual bool next() = 0;

    virtual bool isValueNull(const std::string_view name) = 0;

    virtual void getValue(const std::string_view name, bool& result) = 0;

    virtual void getValue(const std::string_view name, int& result) = 0;

    virtual void getValue(const std::string_view name, int64_t& result) = 0;

#ifdef __SIZEOF_INT128__
    virtual void getValue(const std::string_view name, __uint128_t& result) = 0;
#endif

    virtual void getValue(const std::string_view name, double& result) = 0;

    virtual void getValue(const std::string_view name, std::string& result) = 0;

    virtual void getValue(const std::string_view name, std::vector<uint8_t>& result) = 0;

    virtual void getValue(const std::string_view name, orm::date& value) {
      std::string str;
      getValue(name, str);
      value = str;
    }

    virtual void getValue(const std::string_view name, orm::time& value) {
      std::string str;
      getValue(name, str);
      value = str;
    }

    virtual void getValue(const std::string_view name, orm::datetime& value) {
      std::string str;
      getValue(name, str);
      value = str;
    }

    virtual int columnCount() = 0;

    virtual std::string columnName(int i) = 0;
  };

  class statement {
  public:
    virtual ~statement() {
    }

    virtual std::shared_ptr<resultset> execute() = 0;

    virtual int executeUpdate() = 0;

    virtual void setParamToNull(const std::string_view name) = 0;

    virtual void setParam(const std::string_view name, bool value) = 0;

    virtual void setParam(const std::string_view name, int value) = 0;

    virtual void setParam(const std::string_view name, int64_t value) = 0;

#ifdef __SIZEOF_INT128__
    virtual void setParam(const std::string_view name, __uint128_t value) = 0;
#endif

    virtual void setParam(const std::string_view name, double value) = 0;

    virtual void setParam(const std::string_view name, const std::string_view value) = 0;

    virtual void setParam(const std::string_view name, const std::vector<uint8_t>& value) = 0;

    virtual void setParam(const std::string_view name, orm::date value) {
      setParam(name, (std::string)value);
    }

    virtual void setParam(const std::string_view name, orm::time value) {
      setParam(name, (std::string)value);
    }

    virtual void setParam(const std::string_view name, orm::datetime value) {
      setParam(name, (std::string)value);
    }

    // template <typename T>
    // void setParam(const std::string_view name, const std::vector<T>& vector) {
    //   size_t i = 0;
    //   for (auto& value : vector) {
    //     setParam(name + std::string{"__"} + std::to_string(i++), value);
    //   }
    // }
  };

  class connection {
  public:
    virtual ~connection() {
    }

    virtual std::shared_ptr<statement> prepareStatement(const std::string_view script) = 0;

    virtual void beginTransaction() = 0;

    virtual void commit() = 0;

    virtual void rollback() = 0;

    virtual void execute(const std::string_view script) {
      prepareStatement(script)->execute();
    }

    virtual int getVersion() {
      return -1;
    }

    virtual void setVersion(int version) {
      return;
    }

    virtual bool supportsORM() {
      return false;
    }

    virtual std::string createInsertScript(const char* table, const orm::field_info* fields, size_t fields_len) {
      return {};
    }

    virtual std::string createUpdateScript(const char* table, const orm::field_info* fields, size_t fields_len) {
      return {};
    }

    virtual std::string createUpdateScript(const orm::query_builder_data& data) {
      return {};
    }

    virtual std::string createSelectScript(const orm::query_builder_data& data) {
      return {};
    }

    virtual std::string createDeleteScript(const orm::query_builder_data& data) {
      return {};
    }
  };

  virtual ~datasource() {
  }

  virtual std::shared_ptr<connection> getConnection() = 0;

  virtual std::function<void(db::connection&)>& onConnectionOpen() {
    return _onConnectionOpen;
  }

  virtual std::function<void(db::connection&)>& onConnectionClose() {
    return _onConnectionClose;
  }

  virtual std::vector<orm::update>& updates() {
    return _updates;
  }

private:
  std::function<void(db::connection&)> _onConnectionOpen;
  std::function<void(db::connection&)> _onConnectionClose;

  std::vector<orm::update> _updates;
};

// class pooled_datasource : public datasource {
// public:
//   struct pooled_connection {
//   public:
//     std::shared_ptr<datasource::connection> connection;
//     std::chrono::system_clock::time_point open_since = std::chrono::system_clock::now();
//     bool in_use = false;
//   };

//   virtual std::shared_ptr<connection> getPooledConnection() override {

//   }

// private:

// };
} // namespace db
