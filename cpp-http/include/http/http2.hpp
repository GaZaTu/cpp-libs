#pragma once

#include "./common.hpp"
#include "./zlib.hpp"
#include <cstring>
#include <functional>
#include <nghttp2/nghttp2.h>
#ifndef HTTPPP_NO_TASK
#include "../task.hpp"
#endif

namespace http2 {
// either http::request or http::response
template <typename T>
struct handler {
public:
  handler() {
    _result.version = {2, 0};

    if constexpr (type == HTTP_REQUEST) {
      _result.method = (http_method)-1;
      _result.url.schema = "https";
      _result.url.port = 443;
    } else {
      _result.status = (http_status)-1;
    }

    nghttp2_session_callbacks_new(&_callbacks);

    nghttp2_session_callbacks_set_send_callback(
        _callbacks, [](nghttp2_session* session, const uint8_t* data, size_t length, int flags, void* user_data) {
          std::string_view input{(const char*)data, length};

          auto handler = (http2::handler<T>*)user_data;
          handler->_on_send(input);

          return (ssize_t)length;
        });

    nghttp2_session_callbacks_set_on_header_callback(_callbacks,
        [](nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
            const uint8_t* value, size_t valuelen, uint8_t flags, void* user_data) {
          std::string header_name{(const char*)name, namelen};
          std::string header_value{(const char*)value, valuelen};

          auto handler = (http2::handler<T>*)user_data;

          if constexpr (type == HTTP_REQUEST) {
            if (header_name == ":method") {
              handler->_result.method = (http_method)std::stoi(header_value);
              return 0;
            }
            if (header_name == ":scheme") {
              handler->_result.url.schema = header_value;
              return 0;
            }
            if (header_name == ":authority") {
              handler->_result.url.host = header_value;
              return 0;
            }
            if (header_name == ":path") {
              http::url path{header_value, http::url::IN};
              handler->_result.url.path = std::move(path.path);
              handler->_result.url.query = std::move(path.query);
              handler->_result.url.fragment = std::move(path.fragment);
              return 0;
            }
          } else {
            if (header_name == ":status") {
              handler->_result.status = (http_status)std::stoi(header_value);
              return 0;
            }
          }

          handler->_result.headers[std::move(header_name)] = std::move(header_value);

          return 0;
        });

    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(_callbacks,
        [](nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data, size_t len,
            void* user_data) {
          std::string_view chunk{(const char*)data, len};

          auto handler = (http2::handler<T>*)user_data;
          handler->_result.body += chunk;

          return 0;
        });

    if constexpr (type == HTTP_REQUEST) {
      nghttp2_session_callbacks_set_on_frame_recv_callback(
          _callbacks, [](nghttp2_session* session, const nghttp2_frame* frame, void* user_data) {
            auto handler = (http2::handler<T>*)user_data;
            handler->onStreamClose();

            return 0;
          });
    } else {
      nghttp2_session_callbacks_set_on_stream_close_callback(
          _callbacks, [](nghttp2_session* session, int32_t stream_id, uint32_t error_code, void* user_data) {
            nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR);

            auto handler = (http2::handler<T>*)user_data;
            handler->onStreamClose();

            return 0;
          });
    }

    nghttp2_session_client_new(&_session, _callbacks, this);
  }

  ~handler() {
    nghttp2_session_del(_session);
    nghttp2_session_callbacks_del(_callbacks);
  }

  void execute(std::string_view chunk) {
    int rv = nghttp2_session_mem_recv(_session, (const uint8_t*)chunk.data(), chunk.length());
    if (rv < 0) {
      throw http::error{nghttp2_strerror(rv)};
    }

    if (rv < chunk.length()) {
      throw http::error{"unexpected"};
    }
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

  void onSend(std::function<void(std::string_view)> on_send) {
    _on_send = std::move(on_send);
  }

  void submitSettings() {
    int rv = nghttp2_submit_settings(_session, NGHTTP2_FLAG_NONE, nullptr, 0);
    if (rv != 0) {
      throw http::error{nghttp2_strerror(rv)};
    }
  }

  void submitRequest(http::request& request) {
    std::string method{request.methodAsString()};
    std::string scheme{request.url.schema};
    std::string authority{request.url.host};
    std::string path{request.url.fullpath()};

    short pseudo_headers_len = 4;
    size_t headers_len = pseudo_headers_len + request.headers.size();
    nghttp2_nv* headers = new nghttp2_nv[headers_len];

    headers[0] = makeNV(":method", method);
    headers[1] = makeNV(":scheme", scheme);
    headers[2] = makeNV(":authority", authority);
    headers[3] = makeNV(":path", path);

    size_t i = pseudo_headers_len;
    for (const auto& [name, value] : request.headers) {
      headers[i++] = makeNV(name, value);
    }

    int id = nghttp2_submit_request(_session, 0, headers, headers_len, nullptr, this);
    if (id <= 0) {
      throw http::error{nghttp2_strerror(id)};
    }
  }

  void submitResponse(http::response& response) {
    std::string status{std::to_string(response.status)};

    short pseudo_headers_len = 1;
    size_t headers_len = pseudo_headers_len + response.headers.size();
    nghttp2_nv* headers = new nghttp2_nv[headers_len];

    headers[0] = makeNV(":status", status);

    size_t i = pseudo_headers_len;
    for (const auto& [name, value] : response.headers) {
      headers[i++] = makeNV(name, value);
    }

    nghttp2_data_provider data_provider;
    data_provider.source = {0, &response};
    data_provider.read_callback = [](nghttp2_session* session, int32_t stream_id, uint8_t* buf, size_t length,
                                      uint32_t* data_flags, nghttp2_data_source* source, void* user_data) {
      auto response = (http::response*)user_data;

      auto& offset = (unsigned int)source->fd;
      size_t copy_length = std::min(response->body.length(), length);
      memcpy(buf, response->body.data() + offset, copy_length);
      offset += copy_length;

      return copy_length;
    };

    int id = nghttp2_submit_response(_session, 0, headers, headers_len, &data_provider);
    if (id <= 0) {
      throw http::error{nghttp2_strerror(id)};
    }
  }

  void sendSession() {
    int rv = nghttp2_session_send(_session);
    if (rv != 0) {
      throw http::error{nghttp2_strerror(rv)};
    }
  }

private:
  static constexpr http_parser_type type = std::is_same_v<T, http::request> ? HTTP_REQUEST : HTTP_RESPONSE;

  nghttp2_session_callbacks* _callbacks;
  nghttp2_session* _session;

  bool _done = false;
  std::function<void(T&)> _on_complete;

  T _result;

  std::function<void(std::string_view)> _on_send;

  int onStreamClose() {
    if (_result.headers["content-encoding"] == "gzip") {
      int rc = http::uncompress(_result.body);
      if (rc != 0) {
        return rc;
      }
    }

    if constexpr (type == HTTP_REQUEST) {
      _done = _result.method != -1;
    } else {
      _done = _result.status != -1;
    }

    if (_on_complete) {
      _on_complete(_result);
    }

    return 0;
  }

  nghttp2_nv makeNV(std::string_view name, std::string_view value) {
    return {(uint8_t*)name.data(), (uint8_t*)value.data(), name.length(), value.length(), NGHTTP2_NV_FLAG_NONE};
  }
};
} // namespace http2
