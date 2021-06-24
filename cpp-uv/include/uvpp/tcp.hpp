#pragma once

#include "./dns.hpp"
#include "./error.hpp"
#include "./stream.hpp"
#ifndef UVPP_NO_TASK
#include "../task.hpp"
#endif
#include "uv.h"
#include <functional>

namespace uv {
struct tcp : public stream {
public:
  struct data : public stream::data {
    uv_tcp_t* _native_tcp;
    std::function<void()> tcp_cb;

    data(uv_tcp_t* native_tcp) : _native_tcp(native_tcp) {
    }

    virtual ~data() {
      delete _native_tcp;
    }
  };

  tcp(uv_loop_t* native_loop, uv_tcp_t* native_tcp)
      : stream(native_tcp, new data(native_tcp)), _native_tcp(native_tcp) {
    error::test(uv_tcp_init(native_loop, native_tcp));
  }

  tcp(uv_loop_t* native_loop) : tcp(native_loop, new uv_tcp_t()) {
  }

  tcp(uv_tcp_t* native_tcp) : tcp(uv_default_loop(), native_tcp) {
  }

  tcp() : tcp(uv_default_loop(), new uv_tcp_t()) {
  }

  operator uv_tcp_t*() noexcept {
    return _native_tcp;
  }

  operator const uv_tcp_t*() const noexcept {
    return _native_tcp;
  }

  void accept(tcp& client, std::function<void(uv::error)> cb) {
    stream::accept(client);

#ifndef UVPP_NO_SSL
    if (!_ssl_context) {
      cb(uv::error{0});
      return;
    }

    client.useSSL(*_ssl_context);
    client.hookSSLIntoStream(cb)(uv::error{0});
#else
    cb(uv::error{0});
#endif
  }

#ifndef UVPP_NO_TASK
  task<void> accept(tcp& client) {
    return task<void>::create([this, &client](auto& resolve, auto& reject) {
      accept(client, [&resolve, &reject](auto error) {
        if (error) {
          reject(std::make_exception_ptr(error));
        } else {
          resolve();
        }
      });
    });
  }
#endif

  void nodelay(bool enable) {
    uv_tcp_nodelay(*this, enable);
  }

  void simultaneousAccepts(bool enable) {
    uv_tcp_simultaneous_accepts(*this, enable);
  }

  void bind(const sockaddr* addr, unsigned int flags = 0) {
    error::test(uv_tcp_bind(*this, addr, flags));
  }

  void bind4(const char* ip, int port, unsigned int flags = 0) {
    sockaddr_in addr;
    uv_ip4_addr(ip, port, &addr);
    bind((const sockaddr*)&addr, flags);
  }

  void bind6(const char* ip, int port, unsigned int flags = 0) {
    sockaddr_in6 addr;
    uv_ip6_addr(ip, port, &addr);
    bind((const sockaddr*)&addr, flags);
  }

  // uv_tcp_getsockname

  // uv_tcp_getpeername

  void connect(uv::dns::addrinfo addr, std::function<void(uv::error)> cb) {
    auto _connect = [this](uv::dns::addrinfo addr, std::function<void(uv::error)>& cb) {
      struct data_t : public uv::detail::req::data {
        std::function<void(uv::error)> cb;
      };
      using req_t = uv::req<uv_connect_t, data_t>;

      auto req = new req_t();
      auto data = req->dataPtr();
      data->cb = cb;

      error::test(uv_tcp_connect(*req, *this, addr->ai_addr, [](uv_connect_t* req, int status) {
        auto data = req_t::dataPtr(req);
        auto cb = std::move(data->cb);
        delete data->req;

        cb(uv::error{status});
      }));
    };

#ifndef UVPP_NO_SSL
    if (!_ssl_state) {
      _connect(addr, cb);
      return;
    }

    auto cb_ssl = hookSSLIntoStream(cb);
    _connect(addr, cb_ssl);
#else
    _connect(addr, cb);
#endif
  }

#ifndef UVPP_NO_TASK
  task<void> connect(uv::dns::addrinfo addr) {
    return task<void>::create([this, &addr](auto& resolve, auto& reject) {
      connect(addr, [&resolve, &reject](auto error) {
        if (error) {
          reject(std::make_exception_ptr(error));
        } else {
          resolve();
        }
      });
    });
  }
#endif

  void connect(std::string_view node, std::string_view service, std::function<void(uv::error)> cb) {
    uv::dns::getaddrinfo(node, service, [this, cb](auto addr, auto error) {
      if (error) {
        cb(error);
        return;
      }

      connect(addr, cb);
    });
  }

#ifndef UVPP_NO_TASK
  task<void> connect(std::string_view node, std::string_view service) {
    auto addr = co_await uv::dns::getaddrinfo(node, service);

    co_await connect(addr);
  }
#endif

  void connect(std::string_view node, short port, std::function<void(uv::error)> cb) {
    connect(node.data(), std::to_string(port).data(), cb);
  }

#ifndef UVPP_NO_TASK
  task<void> connect(std::string_view node, short port) {
    co_await connect(node, std::to_string(port));
  }
#endif

private:
  uv_tcp_t* _native_tcp;

  std::function<void(uv::error)> hookSSLIntoStream(std::function<void(uv::error)>& cb) {
    _ssl_state.onReadDecrypted([this](auto data) {
      auto data_ptr = getData<uv::tcp::data>();
      data_ptr->read_decrypted_cb(data, uv::error{0});
    });

    _ssl_state.onWriteEncrypted([this](auto&& input, auto cb) {
      write(
          std::move(input),
          [cb{std::move(cb)}](auto error) {
            if (error) {
              cb(std::make_exception_ptr(error));
            } else {
              cb(nullptr);
            }
          },
          false);
    });

    return [this, cb{std::move(cb)}](auto error) {
      if (error) {
        cb(error);
        return;
      }

      _ssl_state.handshake([this, cb]() {
        cb(uv::error{0});
      });

      readStart(
          [this, cb](auto data, auto error) {
            if (error) {
              auto data_ptr = getData<uv::tcp::data>();

              if (data_ptr->read_decrypted_cb) {
                data_ptr->read_decrypted_cb(std::string_view{nullptr, 0}, error);
              } else if (cb) {
                cb(error);
              }
            } else {
              _ssl_state.decrypt(data);
            }
          },
          false);
    };
  }
};
} // namespace uv
