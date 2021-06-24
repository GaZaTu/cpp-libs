#pragma once

#include "./db/datasource.hpp"
#include <libpq-fe.h>
#include <set>
#include <sstream>

namespace db::pq {
namespace types {
constexpr int _auto = 0;

constexpr int _bool = 16;
constexpr int _bytea = 17;
constexpr int _char = 18;
constexpr int _name = 19;
constexpr int _int8 = 20; // 64-bit
constexpr int _int2 = 21; // 16-bit
constexpr int _int4 = 23; // 32-bit
constexpr int _text = 25;
constexpr int _oid = 26;
constexpr int _xid = 28;
constexpr int _cid = 29;

constexpr int _float4 = 700; // 32-bit
constexpr int _float8 = 701; // 64-bit
constexpr int _unknown = 705;

constexpr int _date = 1082;
constexpr int _time = 1083;

constexpr int _timestamp = 1114;
constexpr int _timestamptz = 1184;

constexpr int _bit = 1560;

constexpr int _numeric = 1700;

constexpr int _uuid = 2950;
} // namespace types

class pq_error : public db::sql_error {
public:
  pq_error(const std::string& msg) : db::sql_error(msg) {
  }

  pq_error(std::shared_ptr<PGconn> db) : db::sql_error(PQerrorMessage(&*db)) {
  }

  pq_error(std::shared_ptr<PGresult> res) : db::sql_error(PQresultErrorMessage(&*res)) {
  }

  static void assert(std::shared_ptr<PGconn> db) {
    auto status = PQstatus(&*db);
    if (status != CONNECTION_OK) {
      throw pq_error(db);
    }
  }

  static void assert(std::shared_ptr<PGresult> res) {
    auto status = PQresultStatus(&*res);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
      throw pq_error(res);
    }
  }
};

class datasource : public db::datasource {
public:
  class resultset : public db::datasource::resultset {
  public:
    resultset(std::shared_ptr<PGconn> native_connection, std::shared_ptr<PGresult> native_resultset)
        : _native_connection(native_connection), _native_resultset(native_resultset) {
    }

    virtual ~resultset() override {
    }

    bool next() override {
      _row += 1;

      return PQntuples(&*_native_resultset) > _row;
    }

    bool isValueNull(const std::string_view name) override {
      return PQgetisnull(&*_native_resultset, _row, columnIndex(name));
    }

    void getValue(const std::string_view name, bool& result) override {
      result = *PQgetvalue(&*_native_resultset, _row, columnIndex(name)) != 'f';
    }

    void getValue(int col, int& result) {
      result = std::atoi(PQgetvalue(&*_native_resultset, _row, col));
    }

    void getValue(const std::string_view name, int& result) override {
      getValue(columnIndex(name), result);
    }

    void getValue(const std::string_view name, int64_t& result) override {
      result = std::atoll(PQgetvalue(&*_native_resultset, _row, columnIndex(name)));
    }

#ifdef __SIZEOF_INT128__
    void getValue(const std::string_view name, __uint128_t& result) override {
      const void* bytes;
      getValue(name, &bytes);
      result = *(__uint128_t*)bytes;
    }
#endif

    void getValue(const std::string_view name, double& result) override {
      result = std::atof(PQgetvalue(&*_native_resultset, _row, columnIndex(name)));
    }

    void getValue(const std::string_view name, std::string& result) override {
      auto col = columnIndex(name);
      auto text_ptr = PQgetvalue(&*_native_resultset, _row, col);
      auto byte_count = PQgetlength(&*_native_resultset, _row, col);

      result = {reinterpret_cast<const char*>(text_ptr), static_cast<std::string::size_type>(byte_count)};
    }

    void getValue(const std::string_view name, std::vector<uint8_t>& result) override {
      auto col = columnIndex(name);
      auto blob_ptr = PQgetvalue(&*_native_resultset, _row, col);
      auto byte_count = PQgetlength(&*_native_resultset, _row, col);
      auto bytes_ptr = reinterpret_cast<const uint8_t*>(blob_ptr);

      result = {bytes_ptr, bytes_ptr + byte_count};
    }

    void getValue(const std::string_view name, const void** result) {
      *result = (const void*)PQgetvalue(&*_native_resultset, _row, columnIndex(name));
    }

    int columnCount() override {
      return PQnfields(&*_native_resultset);
    }

    std::string columnName(int i) override {
      return PQfname(&*_native_resultset, i);
    }

    int columnIndex(const std::string_view name) {
      return PQfnumber(&*_native_resultset, name.data());
    }

  private:
    std::shared_ptr<PGconn> _native_connection;
    std::shared_ptr<PGresult> _native_resultset;

    int _row = -1;
  };

  class statement : public db::datasource::statement {
  public:
    statement(std::shared_ptr<PGconn> native_connection, const std::string_view script)
        : _native_connection(native_connection) {
      _statement = replaceNamedParams((std::string)script);
      _statement_hash = std::to_string(std::hash<std::string>{}(_statement));

      resizeParams(_params_map.size());
    }

    virtual ~statement() override {
    }

    std::shared_ptr<db::datasource::resultset> execute() override {
      return std::make_shared<resultset>(_native_connection, pgPrepareAndExec());
    }

    std::shared_ptr<PGresult> pgPrepareAndExec() {
      if (prepared.count(_statement_hash) == 0) {
        prepared.emplace(_statement_hash);

        std::shared_ptr<PGresult> prep_result{PQprepare(&*_native_connection, _statement_hash.data(), _statement.data(),
                                                  _params.size(), _param_types.data()),
            &PQclear};
        pq_error::assert(prep_result);
      }

      std::shared_ptr<PGresult> exec_result{
          PQexecPrepared(&*_native_connection, _statement_hash.data(), _params.size(), _param_pointers.data(),
              _param_lengths.data(), _param_formats.data(), types::_auto),
          &PQclear};
      pq_error::assert(exec_result);

      return exec_result;
    }

    int executeUpdate() override {
      pgPrepareAndExec();

      return -1;
    }

    void setParamToNull(const std::string_view name) override {
      _param_pointers[pushParam(name, "", types::_auto)] = nullptr;
    }

    void setParam(const std::string_view name, bool value) override {
      pushParam(name, std::to_string(value), types::_bool);
    }

    void setParam(const std::string_view name, int value) override {
      pushParam(name, std::to_string(value), types::_int4);
    }

    void setParam(const std::string_view name, int64_t value) override {
      pushParam(name, std::to_string(value), types::_int8);
    }

#ifdef __SIZEOF_INT128__
    void setParam(const std::string_view name, __uint128_t value) override {
      setParam(name, (const void*)&value, sizeof(__uint128_t));
    }
#endif

    void setParam(const std::string_view name, double value) override {
      pushParam(name, std::to_string(value), types::_float8);
    }

    void setParam(const std::string_view name, const std::string_view value) override {
      pushParam(name, (std::string)value, types::_text);
    }

    void setParam(const std::string_view name, const std::vector<uint8_t>& value) override {
      pushParam(name, (const char*)value.data(), types::_bytea, BINARY, value.size());
    }

    void setParam(const std::string_view name, orm::date value) override {
      pushParam(name, (std::string)value, types::_date);
    }

    void setParam(const std::string_view name, orm::time value) override {
      pushParam(name, (std::string)value, types::_time);
    }

    void setParam(const std::string_view name, orm::datetime value) override {
      pushParam(name, (std::string)value, types::_timestamp);
    }

    void setParam(const std::string_view name, const void* data, int size) {
      pushParam(name, (const char*)data, types::_bit, BINARY, size);
    }

  private:
    static constexpr int TEXT = 0;
    static constexpr int BINARY = 1;

    static std::set<std::string> prepared;

    std::shared_ptr<PGconn> _native_connection;

    std::string _statement;
    std::string _statement_hash;

    std::unordered_map<std::string, size_t> _params_map;

    std::vector<std::string> _params;
    std::vector<Oid> _param_types;
    std::vector<const char*> _param_pointers;
    std::vector<int> _param_lengths;
    std::vector<int> _param_formats;

    void resizeParams(size_t n) {
      _params.resize(n);

      _param_types.resize(n);
      _param_pointers.resize(n);
      _param_lengths.resize(n);
      _param_formats.resize(n);
    }

    int pushParam(std::string_view name, std::string&& value, Oid type, int format = TEXT, int size = -1) {
      size_t i = _params_map[(std::string)name];

      _params[i] = std::move(value);
      value = _params[i];

      if (size == -1) {
        size = value.length();
      }

      _param_types[i] = type;
      _param_pointers[i] = value.data();
      _param_lengths[i] = size;
      _param_formats[i] = format;

      return i;
    }

    std::string replaceNamedParams(std::string&& script) {
      bool in_string = false;
      size_t idx_param = -1;

      for (size_t i = 0; i <= script.length(); i++) {
        switch (script[i]) {
        case '\'':
          if (script[i - 1] != '\\') {
            in_string = !in_string;
          }
          break;
        case ':':
          if (!in_string) {
            if (idx_param != -1) {
            } else if (script[i - 1] != ':') {
              char nc = script[i + 1];
              bool is_digit = (nc >= '0' && nc <= '9');
              bool is_ascii = (nc >= 'a' && nc <= 'z') || (nc >= 'A' && nc <= 'Z');

              if (is_digit || is_ascii) {
                idx_param = i;
              }

              break;
            }
          }
        case ' ':
        case ',':
        case ')':
        case '\0':
          if (idx_param != -1) {
            size_t len = i - idx_param;
            size_t idx = 0;

            std::string name = script.substr(idx_param, len);

            auto search = _params_map.find(name);
            if (search != _params_map.end()) {
              idx = search->second;
            } else {
              idx = _params_map.size();
              _params_map[std::move(name)] = idx;
            }

            std::string key = std::string{'$'} + std::to_string(idx + 1);

            script.replace(idx_param, len, key);

            i = idx_param + key.length();
            idx_param = -1;
          }
        }
      }

      return script;
    }
  };

  class connection : public db::datasource::connection {
  public:
    connection(const std::string_view conninfo) {
      _native_connection = std::shared_ptr<PGconn>{PQconnectdb(conninfo.data()), &PQfinish};
      pq_error::assert(_native_connection);
    }

    virtual ~connection() override {
    }

    std::shared_ptr<db::datasource::statement> prepareStatement(const std::string_view script) override {
      return std::make_shared<statement>(_native_connection, script);
    }

    void beginTransaction() override {
      execute("BEGIN TRANSACTION");
    }

    void commit() override {
      execute("COMMIT");
    }

    void rollback() override {
      execute("ROLLBACK");
    }

    void execute(const std::string_view script) override {
      std::shared_ptr<PGresult> result{PQexec(&*_native_connection, script.data()), &PQclear};
      pq_error::assert(result);
    }

    int getVersion() override {
      execute("CREATE TABLE IF NOT EXISTS user_version (version INTEGER)");
      execute("INSERT INTO user_version SELECT 0 WHERE (SELECT count(*) FROM user_version) = 0");

      auto statement = prepareStatement("SELECT version FROM user_version");
      auto resultset = std::dynamic_pointer_cast<db::pq::datasource::resultset>(statement->execute());
      resultset->next();

      int version = 0;
      resultset->getValue(0, version);

      return version;
    }

    void setVersion(int version) override {
      auto statement = prepareStatement("UPDATE user_version SET version = :version");
      statement->setParam(":version", version);

      statement->executeUpdate();
    }

    bool supportsORM() override {
      return true;
    }

    std::string createInsertScript(const char* table, const orm::field_info* fields, size_t fields_len) override {
      std::stringstream str;

      str << "INSERT INTO " << '"' << table << '"' << " (";

      for (size_t i = 0; i < fields_len; i++) {
        if (i > 0) {
          str << ", ";
        }

        str << fields[i].name;
      }

      str << ") VALUES (";

      for (size_t i = 0; i < fields_len; i++) {
        if (i > 0) {
          str << ", ";
        }

        str << ":" << fields[i].name;
      }

      str << ")";

      return str.str();
    }

    std::string createUpdateScript(const char* table, const orm::field_info* fields, size_t fields_len) override {
      std::stringstream str;

      str << "UPDATE " << '"' << table << '"' << " SET ";

      for (size_t i = 1; i < fields_len; i++) {
        if (i > 1) {
          str << ", ";
        }

        str << fields[i].name << " = :" << fields[i].name;
      }

      str << " WHERE " << fields[0].name << " = :" << fields[0].name;

      return str.str();
    }

    std::string createUpdateScript(const orm::query_builder_data& data) override {
      std::stringstream str;
      int param = 666;

      str << "UPDATE " << '"' << data.table << '"' << " SET ";

      for (size_t i = 0; i < data.assignments.size(); i++) {
        if (i > 0) {
          str << ", ";
        }

        data.assignments.at(i)->appendToQuery(str, param);
      }

      for (size_t i = 0; i < data.conditions.size(); i++) {
        if (i > 0) {
          str << " AND ";
        } else {
          str << " WHERE ";
        }

        data.conditions.at(i)->appendToQuery(str, param);
      }

      return str.str();
    }

    std::string createSelectScript(const orm::query_builder_data& data) override {
      std::stringstream str;

      str << "SELECT ";

      for (size_t i = 0; i < data.fields.size(); i++) {
        if (i > 0) {
          str << ", ";
        }

        str << data.fields.at(i);
      }

      str << " FROM " << '"' << data.table << '"';

      for (size_t i = 0; i < data.conditions.size(); i++) {
        if (i > 0) {
          str << " AND ";
        } else {
          str << " WHERE ";
        }

        data.conditions.at(i)->appendToQuery(str);
      }

      for (size_t i = 0; i < data.ordering.size(); i++) {
        if (i > 0) {
          str << ", ";
        } else {
          str << " ORDER BY ";
        }

        str << data.ordering.at(i).field;

        switch (data.ordering.at(i).direction) {
        case db::orm::ASCENDING:
          str << " ASC";
          break;
        case db::orm::DESCENDING:
          str << " DESC";
          break;
        }

        switch (data.ordering.at(i).nulls) {
        case db::orm::NULLS_FIRST:
          str << " NULLS FIRST";
          break;
        case db::orm::NULLS_LAST:
          str << " NULLS LAST";
          break;
        }
      }

      if (data.limit > 0) {
        str << " LIMIT " << data.limit;
      }

      return str.str();
    }

    std::string createDeleteScript(const orm::query_builder_data& data) override {
      std::stringstream str;

      str << "DELETE FROM " << '"' << data.table << '"';

      for (size_t i = 0; i < data.conditions.size(); i++) {
        if (i > 0) {
          str << " AND ";
        } else {
          str << " WHERE ";
        }

        data.conditions.at(i)->appendToQuery(str);
      }

      return str.str();
    }

  private:
    std::shared_ptr<PGconn> _native_connection;
  };

  datasource(const std::string_view conninfo) : _conninfo(conninfo) {
  }

  std::shared_ptr<db::datasource::connection> getConnection() override {
    return std::make_shared<connection>(_conninfo);
  }

private:
  std::string _conninfo;
};

std::set<std::string> datasource::statement::prepared;
} // namespace db::pq
