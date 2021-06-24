#pragma once

#include "./common.hpp"
#include "./zlib.hpp"
#include <functional>
#ifndef HTTPPP_NO_TASK
#include "../task.hpp"
#endif

namespace http {
// either http::request or http::response
template <typename T>
struct parser {
public:
  parser() {
    _result.version = {1, 1};

    if constexpr (type == HTTP_REQUEST) {
      _result.method = (http_method)-1;
      _result.url.schema = "http";
      _result.url.port = 80;
    } else {
      _result.status = (http_status)-1;
    }

    http_parser_settings_init(&_settings);
    http_parser_init(&_parser, type);

    _parser.data = (void*)this;

    _settings.on_url = [](http_parser* p, const char* data, size_t len) {
      auto parser = (http::parser<T>*)p->data;

      if constexpr (type == HTTP_REQUEST) {
        parser->_result.url = {std::string_view{data, len}, true};
      }

      return 0;
    };

    _settings.on_header_field = [](http_parser* p, const char* data, size_t len) {
      auto parser = (http::parser<T>*)p->data;

      parser->_header = std::string_view{data, len};
      std::transform(parser->_header.begin(), parser->_header.end(), parser->_header.begin(), [](unsigned char c) {
        return std::tolower(c);
      });

      return 0;
    };

    _settings.on_header_value = [](http_parser* p, const char* data, size_t len) {
      auto parser = (http::parser<T>*)p->data;

      parser->_result.headers[parser->_header] = std::string_view{data, len};

      return 0;
    };

    _settings.on_headers_complete = [](http_parser* p) {
      auto parser = (http::parser<T>*)p->data;

      parser->_result.version = {parser->_parser.http_major, parser->_parser.http_minor};

      if constexpr (type == HTTP_REQUEST) {
        parser->_result.method = (http_method)parser->_parser.method;
      } else {
        parser->_result.status = (http_status)parser->_parser.status_code;
      }

      return 0;
    };

    _settings.on_body = [](http_parser* p, const char* data, size_t len) {
      auto parser = (http::parser<T>*)p->data;

      parser->_result.body += std::string_view{data, len};

      return 0;
    };

    _settings.on_message_complete = [](http_parser* p) {
      auto parser = (http::parser<T>*)p->data;

      if (parser->_result.headers["content-encoding"] == "gzip") {
        int rc = http::uncompress(parser->_result.body);
        if (rc != 0) {
          return rc;
        }
      }

      if constexpr (type == HTTP_REQUEST) {
        parser->_done = parser->_result.method != -1;
      } else {
        parser->_done = parser->_result.status != -1;
      }

      if (parser->_on_complete) {
        parser->_on_complete(parser->_result);
      }

      return 0;
    };
  }

  void execute(std::string_view chunk) {
    size_t offset = http_parser_execute(&_parser, &_settings, chunk.data(), chunk.length());

    if (_parser.http_errno != 0) {
      throw http::error{_parser.http_errno};
    }

    if constexpr (type == HTTP_RESPONSE) {
      _result.upgrade = _parser.upgrade != 0;
      if (_result.upgrade) {
        throw http::error{"unexpected"};
        // return chunk.substr(offset);
      }
    }

    // return {};
  }

  void complete(std::function<void(T&)> on_complete) {
    _on_complete = std::move(on_complete);
  }

#ifndef HTTPPP_NO_TASK
  task<T> complete() {
    return task<T>::create([this](auto& resolve, auto&) {
      complete(resolve);
    });
  }
#endif

  void close() {
    _on_complete(_result);
  }

  T result() const {
    return _result;
  }

  operator bool() const {
    return _done;
  }

private:
  static constexpr http_parser_type type = std::is_same_v<T, http::request> ? HTTP_REQUEST : HTTP_RESPONSE;

  http_parser_settings _settings;
  http_parser _parser;

  bool _done = false;
  std::function<void(T&)> _on_complete;

  T _result;

  std::string _header;
};
} // namespace http
