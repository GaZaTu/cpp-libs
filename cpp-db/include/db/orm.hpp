#pragma once

#include "./resultset.hpp"
#include <ostream>
#include <string>

namespace db {
namespace orm {
template <typename T>
struct field_type_converter {
  static constexpr field_type type = UNKNOWN;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<bool> {
  static constexpr field_type type = BOOLEAN;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<int> {
  static constexpr field_type type = INT32;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<int64_t> {
  static constexpr field_type type = INT64;
  static constexpr bool optional = false;
};

#ifdef __SIZEOF_INT128__
template <>
struct field_type_converter<__uint128_t> {
  static constexpr field_type type = UINT128;
  static constexpr bool optional = false;
};
#endif

template <>
struct field_type_converter<double> {
  static constexpr field_type type = DOUBLE;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<std::string> {
  static constexpr field_type type = STRING;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<std::vector<uint8_t>> {
  static constexpr field_type type = BLOB;
  static constexpr bool optional = false;
};

// template <>
// struct field_type_converter<std::time_t> {
//   static constexpr field_type type = DATETIME;
//   static constexpr bool optional = false;
// };

// template <>
// struct field_type_converter<std::chrono::system_clock::time_point> {
//   static constexpr field_type type = DATETIME;
//   static constexpr bool optional = false;
// };

template <>
struct field_type_converter<db::orm::date> {
  static constexpr field_type type = DATE;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<db::orm::time> {
  static constexpr field_type type = TIME;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<db::orm::datetime> {
  static constexpr field_type type = DATETIME;
  static constexpr bool optional = false;
};

template <typename T>
struct field_type_converter<std::optional<T>> : public field_type_converter<T> {
  static constexpr bool optional = true;
};

enum class condition_operator {
  CUSTOM,
  ASSIGNMENT,
  COMMA,
  AND,
  OR,
  EQUALS,
  NOT_EQUALS,
  LOWER_THAN,
  LOWER_THAN_EQUALS,
  GREATER_THAN,
  GREATER_THAN_EQUALS,
  IS_NULL,
  IS_NOT_NULL,
  IN,
};

std::ostream& operator<<(std::ostream& os, condition_operator op) {
  switch (op) {
  case condition_operator::ASSIGNMENT:
    os << " = ";
    break;
  case condition_operator::AND:
    os << " AND ";
    break;
  case condition_operator::OR:
    os << " OR ";
    break;
  case condition_operator::EQUALS:
    os << " = ";
    break;
  case condition_operator::NOT_EQUALS:
    os << " != ";
    break;
  case condition_operator::LOWER_THAN:
    os << " < ";
    break;
  case condition_operator::LOWER_THAN_EQUALS:
    os << " <= ";
    break;
  case condition_operator::GREATER_THAN:
    os << " > ";
    break;
  case condition_operator::GREATER_THAN_EQUALS:
    os << " >= ";
    break;
  case condition_operator::IS_NULL:
    os << " IS NULL";
    break;
  case condition_operator::IS_NOT_NULL:
    os << " IS NOT NULL";
    break;
  }

  return os;
}

template <typename T>
struct is_condition_or_field {
  static constexpr bool value = false;
};

template <typename L, condition_operator O, typename R>
struct condition : public condition_container {
  L left;
  R right;

  condition(L&& l, R&& r) : condition_container(), left(std::move(l)), right(std::move(r)) {
  }

  condition(const L& l, R& r) : condition_container(), left(l), right(std::move(r)) {
  }

  condition(L&& l, R& r) : condition_container(), left(std::move(l)), right(std::move(r)) {
  }

  condition(const L& l, const R& r) : condition_container(), left(l), right(r) {
  }

  condition(L&& l, const R& r) : condition_container(), left(std::move(l)), right(r) {
  }

  // template <typename _L, condition_operator _O, typename _R>
  // constexpr condition<condition, condition_operator::COMMA, condition<_L, _O, _R>> operator,(
  //     condition<_L, _O, _R> right) {
  //   return {std::move(*this), std::move(right)};
  // }

  template <typename _L, condition_operator _O, typename _R>
  constexpr condition<condition, condition_operator::AND, condition<_L, _O, _R>> operator&&(
      condition<_L, _O, _R> right) {
    return {std::move(*this), std::move(right)};
  }

  template <typename _L, condition_operator _O, typename _R>
  constexpr condition<condition, condition_operator::OR, condition<_L, _O, _R>> operator||(
      condition<_L, _O, _R> right) {
    return {std::move(*this), std::move(right)};
  }

  void appendToQuery(std::ostream& os, int& i) const override {
    if constexpr (O != condition_operator::ASSIGNMENT) {
      os << "(";
    }

    if constexpr (O != condition_operator::CUSTOM) {
      left.appendToQuery(os, i);
    } else {
      os << left;
    }

    os << O;

    if constexpr (is_condition_or_field<R>::value) {
      right.appendToQuery(os, i);
    } else {
      if constexpr (std::is_same<R, std::nullopt_t>::value) {
      } else if constexpr (O == condition_operator::CUSTOM) {
      } else {
        os << ":" << i++;
      }
    }

    if constexpr (O != condition_operator::ASSIGNMENT) {
      os << ")";
    }
  }

  void assignToParams(db::statement& statement, int& i) const override {
    if constexpr (is_condition_or_field<L>::value) {
      if constexpr (is_condition_or_field<L>::is_condition) {
        left.assignToParams(statement, i);
      }
    }

    if constexpr (is_condition_or_field<R>::value) {
      if constexpr (is_condition_or_field<R>::is_condition) {
        right.assignToParams(statement, i);
      }
    } else {
      if constexpr (std::is_same<R, std::nullopt_t>::value) {
      } else if constexpr (O == condition_operator::CUSTOM) {
        statement.params[right.name] = right.value;
      } else {
        statement.params[std::string{":"} + std::to_string(i++)] = right;
      }
    }
  }
};

template <typename T>
struct field {
  const char* name;

  constexpr condition<field, condition_operator::ASSIGNMENT, T> operator=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::EQUALS, T> operator==(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::IS_NULL, std::nullopt_t> operator==(std::nullopt_t right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::NOT_EQUALS, T> operator!=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::IS_NOT_NULL, std::nullopt_t> operator!=(std::nullopt_t right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::LOWER_THAN, T> operator<(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::LOWER_THAN_EQUALS, T> operator<=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::GREATER_THAN, T> operator>(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::GREATER_THAN_EQUALS, T> operator>=(const T& right) const {
    return {*this, right};
  }

  // constexpr condition<field, condition_operator::IN, T> IN(const std::vector<T>& right) const {
  //   return {*this, right};
  // }

  void appendToQuery(std::ostream& os, int&) const {
    os << name;
  }

  operator const char*() {
    return name;
  }

  operator std::string() {
    return name;
  }

  operator std::string_view() {
    return name;
  }
};

template <typename L, condition_operator O, typename R>
struct is_condition_or_field<condition<L, O, R>> {
  static constexpr bool value = true;
  static constexpr bool is_condition = true;
  static constexpr bool is_field = false;
};

template <typename T>
struct is_condition_or_field<field<T>> {
  static constexpr bool value = true;
  static constexpr bool is_condition = false;
  static constexpr bool is_field = true;
};

template <typename T>
struct fields {
  static constexpr bool specialized = false;
};

template <typename T>
class selector {
public:
  selector(db::connection& connection) : _connection(connection) {
  }

  selector& select() {
    using meta = db::orm::meta<T>;

    for (const auto& existing : meta::class_members) {
      _data.fields.push_back(existing.name);
    }

    return *this;
  }

  selector& select(const std::vector<std::string>& fields) {
    for (const auto& field : fields) {
      assertFieldExists(field);

      _data.fields.push_back(field);
    }

    return *this;
  }

  template <typename L, condition_operator O, typename R>
  selector& where(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.conditions.push_back(ptr);

    return *this;
  }

  selector& orderBy(const std::string_view field, order_by_direction direction = DIRECTION_DEFAULT,
      order_by_nulls nulls = NULLS_DEFAULT) {
    assertFieldExists(field);

    _data.ordering.push_back(query_builder_data::order_by_clause{(std::string)field, direction, nulls});

    return *this;
  }

  selector& limit(int limit) {
    _data.limit = limit;

    return *this;
  }

  void prepare(db::statement& statement) {
    using meta = db::orm::meta<T>;

    if (!_connection._datasource_connection->supportsORM()) {
      throw db::sql_error{"unsupported"};
    }

    _data.table = meta::class_name;

    std::string script = _connection._datasource_connection->createSelectScript(_data);
    statement.prepare(script);

    for (auto condition : _data.conditions) {
      condition->assignToParams(statement);
    }
  }

  std::vector<T> findAll() {
    using meta = db::orm::meta<T>;

    db::statement statement(_connection);
    prepare(statement);

    std::vector<T> result;

    for (db::resultset& resultset : db::resultset{statement}) {
      T value;
      meta::deserialize(resultset, value);
      result.push_back(std::move(value));
    }

    return result;
  }

  std::optional<T> findOne() {
    _data.limit = 1;

    auto list = findAll();

    if (list.empty()) {
      return {};
    }

    return {std::move(list.at(0))};
  }

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;

  void assertFieldExists(const std::string_view field) {
    using meta = db::orm::meta<T>;

    for (const auto& existing : meta::class_members) {
      if (field == existing.name) {
        return;
      }
    }

    throw db::sql_error{"invalid field"};
  }
};

template <typename T>
class deleter {
public:
  deleter(db::connection& connection) : _connection(connection) {
  }

  template <typename L, condition_operator O, typename R>
  deleter& where(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.conditions.push_back(ptr);

    return *this;
  }

  void prepare(db::statement& statement) {
    using meta = db::orm::meta<T>;

    if (!_connection._datasource_connection->supportsORM()) {
      throw db::sql_error{"unsupported"};
    }

    _data.table = meta::class_name;

    std::string script = _connection._datasource_connection->createDeleteScript(_data);
    statement.prepare(script);

    for (auto condition : _data.conditions) {
      condition->assignToParams(statement);
    }
  }

  int executeUpdate() {
    using meta = db::orm::meta<T>;

    db::statement statement(_connection);
    prepare(statement);

    return statement.executeUpdate();
  }

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;
};

template <typename T>
class updater {
public:
  updater(db::connection& connection) : _connection(connection) {
  }

  template <typename L, condition_operator O, typename R>
  updater& set(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.assignments.push_back(ptr);

    return *this;
  }

  template <typename L, condition_operator O, typename R>
  updater& where(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.conditions.push_back(ptr);

    return *this;
  }

  void prepare(db::statement& statement) {
    using meta = db::orm::meta<T>;

    if (!_connection._datasource_connection->supportsORM()) {
      throw db::sql_error{"unsupported"};
    }

    _data.table = meta::class_name;

    std::string script = _connection._datasource_connection->createUpdateScript(_data);
    statement.prepare(script);

    int param = 666;

    for (auto assignment : _data.assignments) {
      assignment->assignToParams(statement, param);
    }

    for (auto condition : _data.conditions) {
      condition->assignToParams(statement, param);
    }
  }

  int executeUpdate() {
    using meta = db::orm::meta<T>;

    db::statement statement(_connection);
    prepare(statement);

    return statement.executeUpdate();
  }

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;
};

template <typename T>
struct param {
  const char* name;
  T value;
};

struct query {
  std::string query;

  template <typename T>
  constexpr condition<std::string, condition_operator::CUSTOM, param<T>> operator<<(param<T>&& p) const {
    return {std::move(query), std::move(p)};
  }

  template <typename T>
  constexpr condition<std::string, condition_operator::CUSTOM, param<T>> bind(const char* name, T&& value) const {
    return *this << param<T>{name, value};
  }
};
} // namespace orm
} // namespace db

#ifndef FOR_EACH
#define FIRST_ARG(N, ...) N
#define FIRST_ARG_IN_ARRAY(N, ...) N,
#define FIRST_ARG_AS_STRING(N, ...) #N
#define FIRST_ARG_AS_STRING_IN_ARRAY(N, ...) #N,

// Make a FOREACH macro
#define FE_00(WHAT)
#define FE_01(WHAT, X) WHAT(X)
#define FE_02(WHAT, X, ...) WHAT(X) FE_01(WHAT, __VA_ARGS__)
#define FE_03(WHAT, X, ...) WHAT(X) FE_02(WHAT, __VA_ARGS__)
#define FE_04(WHAT, X, ...) WHAT(X) FE_03(WHAT, __VA_ARGS__)
#define FE_05(WHAT, X, ...) WHAT(X) FE_04(WHAT, __VA_ARGS__)
#define FE_06(WHAT, X, ...) WHAT(X) FE_05(WHAT, __VA_ARGS__)
#define FE_07(WHAT, X, ...) WHAT(X) FE_06(WHAT, __VA_ARGS__)
#define FE_08(WHAT, X, ...) WHAT(X) FE_07(WHAT, __VA_ARGS__)
#define FE_09(WHAT, X, ...) WHAT(X) FE_08(WHAT, __VA_ARGS__)
#define FE_10(WHAT, X, ...) WHAT(X) FE_09(WHAT, __VA_ARGS__)
#define FE_11(WHAT, X, ...) WHAT(X) FE_10(WHAT, __VA_ARGS__)
#define FE_12(WHAT, X, ...) WHAT(X) FE_11(WHAT, __VA_ARGS__)
#define FE_13(WHAT, X, ...) WHAT(X) FE_12(WHAT, __VA_ARGS__)
#define FE_14(WHAT, X, ...) WHAT(X) FE_13(WHAT, __VA_ARGS__)
#define FE_15(WHAT, X, ...) WHAT(X) FE_14(WHAT, __VA_ARGS__)
#define FE_16(WHAT, X, ...) WHAT(X) FE_15(WHAT, __VA_ARGS__)
#define FE_17(WHAT, X, ...) WHAT(X) FE_16(WHAT, __VA_ARGS__)
#define FE_18(WHAT, X, ...) WHAT(X) FE_17(WHAT, __VA_ARGS__)
#define FE_19(WHAT, X, ...) WHAT(X) FE_18(WHAT, __VA_ARGS__)
#define FE_20(WHAT, X, ...) WHAT(X) FE_19(WHAT, __VA_ARGS__)
//... repeat as needed

#define GET_MACRO(_00, _01, _02, _03, _04, _05, _06, _07, _08, _09, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
    _20, NAME, ...)                                                                                                   \
  NAME
#define FOR_EACH(ACTION, ...)                                                                                     \
  GET_MACRO(_00, __VA_ARGS__, FE_20, FE_19, FE_18, FE_17, FE_16, FE_15, FE_14, FE_13, FE_12, FE_11, FE_10, FE_09, \
      FE_08, FE_07, FE_06, FE_05, FE_04, FE_03, FE_02, FE_01, FE_00)                                              \
  (ACTION, __VA_ARGS__)
#endif

#define DB_ORM_STRINGIFY_META_FIELD(FIELD)                                   \
  {#FIELD, db::orm::field_type_converter<decltype(class_type::FIELD)>::type, \
      db::orm::field_type_converter<decltype(class_type::FIELD)>::optional},

#define DB_ORM_SERIALIZER_STATEMENT_SET(FIELD) statement.params[":" #FIELD] = source.FIELD;

#define DB_ORM_DESERIALIZER_RESULTSET_GET(FIELD) resultset.get(#FIELD, result.FIELD);

#define DB_ORM_FIELD_INIT(FIELD) static constexpr db::orm::field<decltype(class_type::FIELD)> FIELD{#FIELD};

#define DB_ORM_SPECIALIZE(TYPE, FIELDS...)                                                                  \
  namespace db::orm {                                                                                       \
  template <>                                                                                               \
  struct meta<TYPE> {                                                                                       \
    using class_type = TYPE;                                                                                \
    using id_type = decltype(TYPE::FIRST_ARG(FIELDS));                                                      \
                                                                                                            \
    static constexpr bool specialized = true;                                                               \
    static constexpr const char* class_name = #TYPE;                                                        \
    static constexpr db::orm::field_info class_members[] = {FOR_EACH(DB_ORM_STRINGIFY_META_FIELD, FIELDS)}; \
                                                                                                            \
    static id_type getId(const TYPE& source) {                                                              \
      return source.FIRST_ARG(FIELDS);                                                                      \
    }                                                                                                       \
                                                                                                            \
    static void setId(TYPE& target, const id_type& id) {                                                    \
      target.FIRST_ARG(FIELDS) = id;                                                                        \
    }                                                                                                       \
                                                                                                            \
    static void serialize(db::statement& statement, const TYPE& source) {                                   \
      FOR_EACH(DB_ORM_SERIALIZER_STATEMENT_SET, FIELDS)                                                     \
    }                                                                                                       \
                                                                                                            \
    static void deserialize(db::resultset& resultset, TYPE& result) {                                       \
      FOR_EACH(DB_ORM_DESERIALIZER_RESULTSET_GET, FIELDS)                                                   \
    }                                                                                                       \
  };                                                                                                        \
                                                                                                            \
  template <>                                                                                               \
  struct fields<TYPE> {                                                                                     \
    using class_type = TYPE;                                                                                \
                                                                                                            \
    static constexpr bool specialized = true;                                                               \
                                                                                                            \
    FOR_EACH(DB_ORM_FIELD_INIT, FIELDS)                                                                     \
  };                                                                                                        \
  }

namespace db {
template <typename T>
void serialize(db::statement& statement, const T& source) {
  db::orm::meta<T>::serialize(statement, source);
}

template <typename T>
void deserialize(db::resultset& resultset, T& result) {
  db::orm::meta<T>::deserialize(resultset, result);
}
} // namespace db

namespace db::orm {
// template <typename ...A>
// struct call {
//   const char* name;
//   std::tuple<A...> args;

//   template <typename T>
//   constexpr condition<call, EQUALS, T> operator==(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, IS_NULL, std::nullopt_t> operator==(std::nullopt_t right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, NOT_EQUALS, T> operator!=(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, IS_NOT_NULL, std::nullopt_t> operator!=(std::nullopt_t right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, LOWER_THAN, T> operator<(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, LOWER_THAN_EQUALS, T> operator<=(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, GREATER_THAN, T> operator>(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, GREATER_THAN_EQUALS, T> operator>=(const T& right) const {
//     return {std::move(*this), right};
//   }

//   void appendToQuery(std::ostream& os, int& i) const {
//     os << name << "(";

//     std::apply([&os](auto&&... args) {((os << args), ...);}, args);

//     os << ")";
//   }
// };

// struct func {
//   const char* name;

//   template <typename ...A>
//   constexpr call<A...> operator()(A&&... args) {
//     return {name, std::forward_as_tuple(args...)};
//   }
// };
} // namespace db::orm
