#pragma once

#include "./connection.hpp"
#include "./orm.hpp"

namespace db::orm {
class repository {
public:
  repository(db::connection& conn) : _conn(conn) {
  }

  template <typename T>
  std::optional<T> findOneById(const typename db::orm::meta<T>::id_type& id) {
    using meta = db::orm::meta<T>;
    using id_type = typename meta::id_type;

    constexpr auto id_field_name = meta::class_members[0].name;
    constexpr auto id_field = db::orm::field<id_type>{id_field_name};

    return db::orm::selector<T>{_conn}.select().where(id_field = id).findOne();
  }

  template <typename T>
  std::optional<T> findOneById(const std::optional<typename db::orm::meta<T>::id_type>& id) {
    if (!id) {
      return {};
    }

    return findOneById<T>(*id);
  }

  template <typename T>
  int save(T& source) {
    using meta = db::orm::meta<T>;
    using id = db::orm::id<typename meta::id_type>;

    auto fields = (const db::orm::field_info*)&meta::class_members;
    auto fields_len = sizeof(meta::class_members) / sizeof(db::orm::field_info);

    if (!_conn.getNativeConnection()->supportsORM()) {
      throw db::sql_error{"unsupported"};
    }

    std::string script;

    if (id::isNull(meta::getId(source))) {
      meta::setId(source, id::generate());

      script = _conn.getNativeConnection()->createInsertScript(meta::class_name, fields, fields_len);
    } else {
      script = _conn.getNativeConnection()->createUpdateScript(meta::class_name, fields, fields_len);
    }

    db::statement statement(_conn);
    statement.prepare(script);
    meta::serialize(statement, source);

    return statement.executeUpdate();
  }

  template <typename T>
  int save(std::optional<T>& source) {
    if (source) {
      return save<T>(source.value());
    }

    return 0;
  }

  template <typename T>
  int remove(const T& source) {
    using meta = db::orm::meta<T>;

    return removeById<T>(meta::getId(source));
  }

  template <typename T>
  int remove(const std::optional<T>& source) {
    if (source) {
      return remove<T>(source.value());
    }

    return 0;
  }

  template <typename T>
  int removeById(const typename db::orm::meta<T>::id_type& id) {
    using meta = db::orm::meta<T>;
    using id_type = typename meta::id_type;

    constexpr auto id_field_name = meta::class_members[0].name;
    constexpr auto id_field = db::orm::field<id_type>{id_field_name};

    return db::orm::deleter<T>{_conn}.where(id_field = id).executeUpdate();
  }

  template <typename T>
  int removeById(const std::optional<typename db::orm::meta<T>::id_type>& id) {
    if (!id) {
      return 0;
    }

    return removeById<T>(*id);
  }

private:
  db::connection& _conn;
};
} // namespace db::orm
