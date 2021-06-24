#pragma once

#include "./error.hpp"
#include "./handle.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct async : public handle {
public:
  struct data : public handle::data {
    uv_async_t* _native_async;
    std::function<void()> async_cb;

    data(uv_async_t* native_async) : _native_async(native_async) {
    }

    virtual ~data() {
      delete _native_async;
    }
  };

  async(uv_loop_t* native_loop, uv_async_t* native_async)
      : handle(native_async, new data(native_async)), _native_async(native_async) {
    error::test(uv_async_init(native_loop, native_async, [](uv_async_t* native_async) {
      data* data_ptr = handle::getData<data>(native_async);
      data_ptr->async_cb();
    }));
  }

  async(uv_loop_t* native_loop) : async(native_loop, new uv_async_t()) {
  }

  async(uv_async_t* native_async) : async(uv_default_loop(), native_async) {
  }

  async() : async(uv_default_loop(), new uv_async_t()) {
  }

  async(async&& source) noexcept
      : handle(source._native_async, handle::getData<data>(source._native_async)),
        _native_async(std::exchange(source._native_async, nullptr)) {
  }

  operator uv_async_t*() noexcept {
    return _native_async;
  }

  operator const uv_async_t*() const noexcept {
    return _native_async;
  }

  void send(std::function<void()> async_cb) {
    data* data_ptr = getData<data>();
    data_ptr->async_cb = async_cb;

    error::test(uv_async_send(*this));
  }

#ifndef UVPP_NO_TASK
  task<void> send() {
    return task<void>::create([this](auto& resolve, auto& reject) {
      send(resolve);
    });
  }
#endif

  static void queue(std::function<void()> cb) {
    uv::async* async = new uv::async();
    async->send([async, cb{std::move(cb)}]() {
      delete async;
      cb();
    });
  }

#ifndef UVPP_NO_TASK
  static task<void> queue() {
    return task<void>::create([](auto& resolve, auto& reject) {
      queue(resolve);
    });
  }
#endif

private:
  uv_async_t* _native_async;
};
} // namespace uv
