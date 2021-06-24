#pragma once

#include "./common.hpp"
#include <variant>

namespace irc {
struct ping {
public:
  ping(std::string& raw) : _raw(std::move(raw)) {
    parse();
  }

  ping(std::string_view raw) : _raw(raw) {
    parse();
  }

  ping(const ping& other) {
    *this = other;
  }

  ping(ping&&) = default;

  ping& operator=(const ping& other) {
    _raw = (std::string)other;
    parse();

    return *this;
  }

  ping& operator=(ping&&) = default;

  std::string_view server() const {
    return _views._server;
  }

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

  struct {
    std::string_view _server;
  } _views;

  void parse() {
    std::string_view data = (std::string_view)(*this);

    std::cmatch match;
    std::regex_search(data.begin(), data.end(), match, regex::ping);

    if (match.empty()) {
      if (_raw.index() == STRING) {
        throw parsing_error{std::get<STRING>(_raw)};
      } else {
        throw parsing_error{};
      }
    }

    _views._server = {match[1].first, (size_t)match[1].length()};
  }
};
} // namespace irc
