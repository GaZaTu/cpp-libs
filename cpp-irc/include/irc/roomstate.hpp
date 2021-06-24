#pragma once

#include "./common.hpp"
#include <variant>

namespace irc {
struct roomstate {
public:
  roomstate(std::string& raw) : _raw(std::move(raw)) {
    parse();
  }

  roomstate(std::string_view raw) : _raw(raw) {
    parse();
  }

  roomstate(const roomstate& other) {
    *this = other;
  }

  roomstate(roomstate&&) = default;

  roomstate& operator=(const roomstate& other) {
    _raw = (std::string)other;
    parse();

    return *this;
  }

  roomstate& operator=(roomstate&&) = default;

  std::string_view channel() const {
    return _views._channel;
  }

  std::string_view operator[](std::string_view key) const {
    return _tags.at(key);
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
  std::unordered_map<std::string_view, std::string_view> _tags;

  struct {
    std::string_view _channel;
  } _views;

  void parse() {
    std::string_view data;
    std::tie(data, _tags) = consumeTags((std::string_view)(*this));

    std::cmatch match;
    std::regex_search(data.begin(), data.end(), match, regex::roomstate);

    if (match.empty()) {
      if (_raw.index() == STRING) {
        throw parsing_error{std::get<STRING>(_raw)};
      } else {
        throw parsing_error{};
      }
    }

    _views._channel = {match[1].first, (size_t)match[1].length()};
  }
};
} // namespace irc
