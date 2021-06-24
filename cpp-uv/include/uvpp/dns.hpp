#pragma once

#include "./error.hpp"
#include "./req.hpp"
#include "uv.h"
#ifndef UVPP_NO_TASK
#include "../task.hpp"
#endif
#include <functional>
#include <memory>
#include <variant>

namespace uv {
namespace dns {
// struct addrinfo {
// public:
//   addrinfo(::addrinfo* native) : _native{native, uv_freeaddrinfo} {
//   }

//   operator ::addrinfo*() noexcept {
//     return _native.get();
//   }

//   operator const ::addrinfo*() const noexcept {
//     return _native.get();
//   }

//   ::addrinfo* operator->() noexcept {
//     return _native.get();
//   }

// private:
//   std::shared_ptr<::addrinfo> _native;
// };

using addrinfo = std::shared_ptr<::addrinfo>;

void uv_freeaddrinfo2(::addrinfo* addr) {
  printf("");
}

void getaddrinfo(std::string_view node, std::string_view service, std::function<void(uv::dns::addrinfo, uv::error)> cb,
    uv_loop_t* native_loop = uv_default_loop()) {
  struct data_t : public uv::detail::req::data {
    std::function<void(uv::dns::addrinfo, uv::error)> cb;
  };
  using req_t = uv::req<uv_getaddrinfo_t, data_t>;

  auto req = new req_t();
  auto data = req->dataPtr();
  data->cb = cb;

  // ::addrinfo* hints = new ::addrinfo();
  // hints->ai_family = PF_INET;
  // hints->ai_socktype = SOCK_STREAM;
  // hints->ai_protocol = IPPROTO_TCP;
  // hints->ai_flags = 0;

  error::test(uv_getaddrinfo(
      native_loop, *req,
      [](uv_getaddrinfo_t* req, int status, ::addrinfo* res) {
        auto data = req_t::dataPtr(req);
        auto cb = std::move(data->cb);
        delete data->req;

        cb(uv::dns::addrinfo{res, &uv_freeaddrinfo}, uv::error{status});
      },
      node.data(), service.data(), nullptr));
}

#ifndef UVPP_NO_TASK
task<uv::dns::addrinfo> getaddrinfo(
    std::string_view node, std::string_view service, uv_loop_t* native_loop = uv_default_loop()) {
  return task<uv::dns::addrinfo>::create(
      [node{(std::string)node}, service{(std::string)service}, native_loop](auto& resolve, auto& reject) {
        uv::dns::getaddrinfo(
            node, service,
            [&resolve, &reject](auto result, auto error) {
              if (error) {
                reject(std::make_exception_ptr(error));
              } else {
                resolve(result);
              }
            },
            native_loop);
      });
}
#endif
} // namespace dns
} // namespace uv
