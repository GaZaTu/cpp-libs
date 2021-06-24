#pragma once

#include "./common.hpp"
#include <variant>

namespace irc {
struct unknown {
public:
  unknown() : _raw({}) {
  }

  unknown(std::string& raw) : _raw(std::move(raw)) {
  }

  unknown(std::string_view raw) : _raw(raw) {
  }

  unknown(const unknown& other) {
    *this = other;
  }

  unknown(unknown&&) = default;

  unknown& operator=(const unknown& other) {
    _raw = (std::string)other;

    return *this;
  }

  unknown& operator=(unknown&&) = default;

  explicit operator std::string_view() const {
    if (_raw.index() == VIEW) {
      return std::get<VIEW>(_raw);
    } else {
      return std::get<STRING>(_raw);
    }
  }

  explicit operator std::string() const {
    return (std::string)(std::string_view)(*this);
  }

private:
  static constexpr int VIEW = 0;
  static constexpr int STRING = 1;

  std::variant<std::string_view, std::string> _raw;
};
} // namespace irc
