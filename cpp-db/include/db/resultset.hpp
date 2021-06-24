#pragma once

#include "./statement.hpp"
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace db {
class resultset {
public:
  class iterator {
  public:
    explicit iterator(resultset& rslt, bool done) : _rslt(rslt), _done(done) {
      if (!_done) {
        ++(*this);
      }
    }

    iterator(const iterator&) = default;

    iterator(iterator&&) = default;

    iterator& operator=(const iterator&) = default;

    iterator& operator=(iterator&&) = default;

    iterator& operator++() {
      _done = !_rslt.get().next();
      return *this;
    }

    iterator operator++(int) {
      iterator result = *this;
      ++(*this);
      return result;
    }

    bool operator==(const iterator& other) const {
      return _done == other._done;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    resultset& operator*() {
      return _rslt.get();
    }

  private:
    std::reference_wrapper<resultset> _rslt;
    bool _done;
  };

  class columns_iterable {
  public:
    class iterator {
    public:
      explicit iterator(resultset& rslt, int i, bool done) : _rslt(rslt), _i(i), _done(done) {
        if (!_done) {
          ++(*this);
        }
      }

      iterator(const iterator&) = default;

      iterator(iterator&&) = default;

      iterator& operator=(const iterator&) = default;

      iterator& operator=(iterator&&) = default;

      iterator& operator++() {
        _done = (_i += 1) > _rslt.get()._datasource_resultset->columnCount();

        if (!_done) {
          _column_name = _rslt.get()._datasource_resultset->columnName(_i - 1);
        }

        return *this;
      }

      iterator operator++(int) {
        iterator result = *this;
        ++(*this);
        return result;
      }

      bool operator==(const iterator& other) const {
        return _done == other._done;
      }

      bool operator!=(const iterator& other) const {
        return !(*this == other);
      }

      const std::string& operator*() const {
        return _column_name;
      }

    private:
      std::reference_wrapper<resultset> _rslt;
      int _i;
      bool _done;
      std::string _column_name;
    };

    explicit columns_iterable(resultset& rslt) : _rslt(rslt) {
    }

    columns_iterable(const columns_iterable&) = delete;

    columns_iterable(columns_iterable&&) = default;

    columns_iterable& operator=(const columns_iterable&) = delete;

    columns_iterable& operator=(columns_iterable&&) = default;

    iterator begin() {
      return iterator(_rslt.get(), 0, false);
    }

    iterator end() {
      return iterator(_rslt.get(), -1, true);
    }

  private:
    std::reference_wrapper<resultset> _rslt;
  };

  class polymorphic_field {
  public:
    explicit polymorphic_field(resultset& rslt, const std::string_view name) : _rslt(rslt), _name(name) {
    }

    polymorphic_field(const polymorphic_field&) = delete;

    polymorphic_field(polymorphic_field&&) = default;

    polymorphic_field& operator=(const polymorphic_field&) = delete;

    polymorphic_field& operator=(polymorphic_field&&) = default;

    template <typename T>
    std::optional<T> get() {
      return _rslt.get().get<T>(_name);
    }

    template <typename T>
    operator std::optional<T>() {
      return get<T>();
    }

  private:
    std::reference_wrapper<resultset> _rslt;
    std::string _name;
  };

  explicit resultset(statement& stmt) {
    _datasource_resultset = stmt._datasource_statement->execute();
  }

  resultset(const resultset&) = delete;

  resultset(resultset&&) = delete;

  resultset& operator=(const resultset&) = delete;

  resultset& operator=(resultset&&) = delete;

  bool next() {
    return _datasource_resultset->next();
  }

  template <typename T>
  bool get(const std::string_view name, T& result) {
    if (_datasource_resultset->isValueNull(name)) {
      return false;
    }

    if constexpr (type_converter<T>::specialized) {
      typename type_converter<T>::db_type tmp;

      _datasource_resultset->getValue(name, tmp);

      result = type_converter<T>::deserialize(tmp);
    } else {
      _datasource_resultset->getValue(name, result);
    }

    return true;
  }

  template <typename T>
  inline void get(const std::string_view name, std::optional<T>& result) {
    T value;
    if (get<T>(name, value)) {
      result = std::move(value);
    }
  }

  template <typename T>
  inline std::optional<T> get(const std::string_view name) {
    std::optional<T> result;
    get<T>(name, result);
    return result;
  }

  template <typename T>
  inline T value(const std::string_view name) {
    T result;
    get<T>(name, result);
    return result;
  }

  polymorphic_field operator[](const std::string_view name) {
    return polymorphic_field(*this, name);
  }

  iterator begin() {
    return iterator(*this, false);
  }

  iterator end() {
    return iterator(*this, true);
  }

  columns_iterable columns() {
    return columns_iterable(*this);
  }

  template <typename T>
  std::optional<T> firstValue() {
    for (auto& rslt : *this) {
      for (const auto& col : rslt.columns()) {
        return rslt.get<T>(col);
      }
    }

    return {};
  }

  // template <typename ...T>
  // std::tuple<std::optional<T>...> firstTuple() {
  //   return {};
  // }

  template <typename T = datasource::resultset>
  std::shared_ptr<T> getNativeResultset() {
    return std::dynamic_pointer_cast<T>(_datasource_resultset);
  }

private:
  std::shared_ptr<datasource::resultset> _datasource_resultset;
};

template <typename T>
std::optional<T> connection::execute(const std::string_view script) {
  statement stmt{*this};
  stmt.prepare(script);
  resultset rslt{stmt};

  return rslt.firstValue<T>();
}
} // namespace db
