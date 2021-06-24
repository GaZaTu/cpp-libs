#pragma once

#include "./common.hpp"
#include <variant>

namespace irc {
struct privmsg {
public:
  privmsg(std::string& raw) : _raw(std::move(raw)) {
    parse();
  }

  privmsg(std::string_view raw) : _raw(raw) {
    parse();
  }

  privmsg(const privmsg& other) {
    *this = other;
  }

  privmsg(privmsg&&) = default;

  privmsg& operator=(const privmsg& other) {
    _raw = (std::string)other;
    parse();

    return *this;
  }

  privmsg& operator=(privmsg&&) = default;

  std::string_view channel() const {
    return _views._channel;
  }

  std::string_view sender() const {
    return _views._sender;
  }

  std::string_view message() const {
    return _views._message;
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
    std::string_view _sender;
    std::string_view _message;
  } _views;

  void parse() {
    std::string_view data;
    std::tie(data, _tags) = consumeTags((std::string_view)(*this));

    std::cmatch match;
    std::regex_search(data.begin(), data.end(), match, regex::privmsg);

    if (match.empty()) {
      if (_raw.index() == STRING) {
        throw parsing_error{std::get<STRING>(_raw)};
      } else {
        throw parsing_error{};
      }
    }

    _views._channel = {match[2].first, (size_t)match[2].length()};
    _views._sender = {match[1].first, (size_t)match[1].length()};
    _views._message = {match.suffix().first, (size_t)match.suffix().length()};
  }
};
} // namespace irc
