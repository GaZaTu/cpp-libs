#pragma once

#include "http_parser.h"
#include <sstream>
#include <string>
#include <unordered_map>

namespace http {
enum method {
  DELETE = HTTP_DELETE,
  GET = HTTP_GET,
  HEAD = HTTP_HEAD,
  POST = HTTP_POST,
  PUT = HTTP_PUT,
  CONNECT = HTTP_CONNECT,
  OPTIONS = HTTP_OPTIONS,
  TRACE = HTTP_TRACE,
};

struct url {
public:
  static constexpr bool IN = true;
  static constexpr bool OUT = false;

  std::string schema = "http";
  std::string host;
  int16_t port = 80;
  std::string path = "/";
  std::string query;
  std::string fragment;

  url() {
  }

  url(std::string_view str, bool in = OUT) {
    http_parser_url parser;
    http_parser_url_init(&parser);
    http_parser_parse_url(str.data(), str.length(), in, &parser);

    auto move = [&](http_parser_url_fields field, std::string& member) {
      auto& data = parser.field_data[field];

      if (data.len != 0) {
        member = std::string_view{str.data() + data.off, data.len};
      }
    };

    move(UF_SCHEMA, schema);
    move(UF_HOST, host);
    // move(UF_PORT, port);
    move(UF_PATH, path);
    move(UF_QUERY, query);
    move(UF_FRAGMENT, fragment);

    if (schema == "https") {
      port = 443;
    }

    if (parser.port != 0) {
      port = parser.port;
    }
  }

  url(const char* str) : url(std::string_view{str}) {
  }

  url(const url&) = default;

  url(url&&) = default;

  url& operator=(const url&) = default;

  url& operator=(url&&) = default;

  std::string fullpath() const {
    std::string result = path;

    if (query != "") {
      result += "?" + query;
    }

    if (fragment != "") {
      result += "#" + fragment;
    }

    return result;
  }

  explicit operator std::string() const {
    std::stringstream result;

    if (host != "") {
      result << schema << "://" << host;

      if ((schema == "http" && port != 80) || (schema == "https" && port != 443)) {
        result << ":" << port;
      }
    }

    result << fullpath();

    return result.str();
  }
};

struct request {
public:
  std::tuple<unsigned short, unsigned short> version = {1, 1};

  http::url url;

  http_method method = HTTP_GET;

  std::unordered_map<std::string, std::string> headers;

  std::string body;

  request() {
  }

  request(http_method m, http::url u, std::string b = {}) : method(m), url(u), body(b) {
  }

  request(http::url u) : url(u) {
  }

  explicit operator std::string() const {
    return stringify(*this);
  }

  std::string methodAsString() const {
    return http_method_str((http_method)method);
  }

private:
  static std::string stringify(const request& r, const std::unordered_map<std::string, std::string>& headers = {}) {
    auto url = r.url;
    url.host = "";

    std::stringstream result;

    result << r.methodAsString() << " ";
    result << (std::string)url << " ";
    result << "HTTP/" << std::get<0>(r.version) << "." << std::get<1>(r.version) << "\r\n";

    for (const auto& [key, value] : headers) {
      result << key << ": " << value << "\r\n";
    }

    for (const auto& [key, value] : r.headers) {
      result << key << ": " << value << "\r\n";
    }

    result << "\r\n";

    if (r.body.length() > 0) {
      result << r.body << "\r\n\r\n";
    }

    return result.str();
  }
};

struct response {
public:
  std::tuple<unsigned short, unsigned short> version = {1, 1};

  http_status status = HTTP_STATUS_OK;

  std::unordered_map<std::string, std::string> headers;

  std::string body;

  bool upgrade = false;

  response() {
  }

  response(http_status s, std::string b = {}) : status(s), body(b) {
  }

  operator bool() const {
    return status >= 200 && status < 300;
  }

  explicit operator std::string() const {
    return stringify(*this);
  }

  std::string statusAsString() const {
    return http_status_str((http_status)status);
  }

private:
  static std::string stringify(const response& r, const std::unordered_map<std::string, std::string>& headers = {}) {
    std::stringstream result;

    result << "HTTP/" << std::get<0>(r.version) << "." << std::get<1>(r.version) << " ";
    result << r.status << " ";
    result << r.statusAsString() << "\r\n";

    for (const auto& [key, value] : headers) {
      result << key << ": " << value << "\r\n";
    }

    for (const auto& [key, value] : r.headers) {
      result << key << ": " << value << "\r\n";
    }

    result << "\r\n";

    if (r.body.length() > 0) {
      result << r.body << "\r\n\r\n";
    }

    return result.str();
  }
};

class error : public std::runtime_error {
public:
  error(const std::string& s) : std::runtime_error(s) {
  }

  error(unsigned int c = 0) : std::runtime_error(errno_string(c)) {
  }

  error(const response& r) : std::runtime_error(std::to_string(r.status) + " " + r.statusAsString()) {
  }

private:
  static std::string errno_string(unsigned int code) {
    return std::string{http_errno_name((http_errno)code)} + ": " +
        std::string{http_errno_description((http_errno)code)};
  }
};
} // namespace http
