#pragma once

#include "./common.hpp"
#include <variant>

namespace irc {
struct reconnect {
public:
  reconnect(std::string& raw) : _raw(std::move(raw)) {
    parse();
  }

  reconnect(std::string_view raw) : _raw(raw) {
    parse();
  }

  reconnect(const reconnect& other) {
    *this = other;
  }

  reconnect(reconnect&&) = default;

  reconnect& operator=(const reconnect& other) {
    _raw = (std::string)other;
    parse();

    return *this;
  }

  reconnect& operator=(reconnect&&) = default;

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

  void parse() {
    std::string_view data = (std::string_view)(*this);

    std::cmatch match;
    std::regex_search(data.begin(), data.end(), match, regex::reconnect);

    if (match.empty()) {
      if (_raw.index() == STRING) {
        throw parsing_error{std::get<STRING>(_raw)};
      } else {
        throw parsing_error{};
      }
    }
  }
};
} // namespace irc
