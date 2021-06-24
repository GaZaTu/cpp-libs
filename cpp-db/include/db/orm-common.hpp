#pragma once

#include "./common.hpp"
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

namespace db::orm {
enum field_type {
  UNKNOWN,
  BOOLEAN,
  INT32,
  INT64,
#ifdef __SIZEOF_INT128__
  UINT128,
#endif
  DOUBLE,
  STRING,
  BLOB,
  DATE,
  TIME,
  DATETIME,
};

struct field_info {
  const char* name;
  field_type type = UNKNOWN;
  bool optional = false;
};

namespace detail {
constexpr char format_date[] = "%Y-%m-%d";
constexpr char output_date[] = "YYYY:mm:dd";

constexpr char format_time[] = "%H:%M";
constexpr char output_time[] = "HH:MM";

constexpr char format_iso[] = "%Y-%m-%dT%H:%M:%SZ";
constexpr char output_iso[] = "YYYY-mm-ddTHH:MM:SSZ";

template <const char* FORMAT, size_t LENGTH>
struct timestamp {
  std::time_t value = 0;

  static timestamp now() {
    std::time_t now;
    ::time(&now);
    return now;
  }

  timestamp() {
  }

  timestamp(std::time_t t) {
    *this = t;
  }

  timestamp(std::string_view s) {
    *this = s;
  }

  timestamp& operator=(std::time_t t) {
    value = t;
    return *this;
  }

  timestamp& operator=(std::string_view s) {
    std::istringstream ss(s.data());
    struct std::tm tm;
    ss >> std::get_time(&tm, FORMAT);
    value = mktime(&tm);
    return *this;
  }

  operator std::time_t() {
    return value;
  }

  explicit operator std::string() {
    std::string result;
    result.resize(LENGTH);
    strftime(result.data(), LENGTH, FORMAT, gmtime(&value));
    return result;
  }

  operator bool() {
    return value != 0;
  }
};
} // namespace detail

using date = detail::timestamp<detail::format_date, sizeof(detail::output_date)>;
using time = detail::timestamp<detail::format_time, sizeof(detail::output_time)>;
using datetime = detail::timestamp<detail::format_iso, sizeof(detail::output_iso)>;

template <typename T>
struct meta {
  static constexpr bool specialized = false;
};

template <typename T>
struct id {
  static constexpr bool specialized = false;
};

struct update {
  int version;

  std::function<void(db::connection&)> up;
  std::function<void(db::connection&)> down;
};

enum order_by_direction {
  DIRECTION_DEFAULT,
  ASCENDING,
  DESCENDING,
};

std::ostream& operator<<(std::ostream& os, order_by_direction op) {
  switch (op) {
  case ASCENDING:
    os << "ASC";
    break;
  case DESCENDING:
    os << "DESC";
    break;
  }
  return os;
}

enum order_by_nulls {
  NULLS_DEFAULT,
  NULLS_FIRST,
  NULLS_LAST,
};

std::ostream& operator<<(std::ostream& os, order_by_nulls op) {
  switch (op) {
  case NULLS_FIRST:
    os << "NULLS FIRST";
    break;
  case NULLS_LAST:
    os << "NULLS LAST";
    break;
  }
  return os;
}

struct condition_container {
  virtual ~condition_container() {
  }

  virtual void appendToQuery(std::ostream& os, int& i) const = 0;

  void appendToQuery(std::ostream& os) const {
    int i = 666;
    appendToQuery(os, i);
  }

  virtual void assignToParams(db::statement& statement, int& i) const = 0;

  void assignToParams(db::statement& statement) const {
    int i = 666;
    assignToParams(statement, i);
  }
};

struct query_builder_data {
  struct order_by_clause {
    std::string field;
    order_by_direction direction = DIRECTION_DEFAULT;
    order_by_nulls nulls = NULLS_DEFAULT;
  };

  std::string table;
  std::vector<std::string> fields;
  std::vector<std::shared_ptr<condition_container>> assignments;
  std::vector<std::shared_ptr<condition_container>> conditions;
  std::vector<order_by_clause> ordering;
  int limit = 0;
};

template <typename T>
class selector;

template <typename T>
class deleter;

template <typename T>
class updater;
} // namespace db::orm
