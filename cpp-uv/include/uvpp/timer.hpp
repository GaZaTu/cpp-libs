#pragma once

#include "./error.hpp"
#include "./handle.hpp"
#ifndef UVPP_NO_TASK
#include "../task.hpp"
#endif
#include "uv.h"
#include <functional>

namespace uv {
struct timer : public handle {
public:
  struct data : public handle::data {
    uv_timer_t* _native_timer;
    std::function<void()> timer_cb;

    data(uv_timer_t* native_timer) : _native_timer(native_timer) {
    }

    virtual ~data() {
      delete _native_timer;
    }
  };

  timer(uv_loop_t* native_loop, uv_timer_t* native_timer)
      : handle(native_timer, new data(native_timer)), _native_timer(native_timer) {
    error::test(uv_timer_init(native_loop, native_timer));
  }

  timer(uv_loop_t* native_loop) : timer(native_loop, new uv_timer_t()) {
  }

  timer(uv_timer_t* native_timer) : timer(uv_default_loop(), native_timer) {
  }

  timer() : timer(uv_default_loop(), new uv_timer_t()) {
  }

  timer(timer&& source) noexcept
      : handle(source._native_timer, handle::getData<data>(source._native_timer)),
        _native_timer(std::exchange(source._native_timer, nullptr)) {
  }

  operator uv_timer_t*() noexcept {
    return _native_timer;
  }

  operator const uv_timer_t*() const noexcept {
    return _native_timer;
  }

  void start(std::function<void()> timer_cb, uint64_t timeout, uint64_t repeat = 0) {
    data* data_ptr = getData<data>();
    data_ptr->timer_cb = timer_cb;

    error::test(uv_timer_start(
        *this,
        [](uv_timer_t* native_timer) {
          data* data_ptr = handle::getData<data>(native_timer);
          data_ptr->timer_cb();
        },
        timeout, repeat));
  }

  void startOnce(uint64_t timeout, std::function<void()> cb) {
    start(cb, timeout, 0);
  }

#ifndef UVPP_NO_TASK
  task<void> startOnce(uint64_t timeout) {
    return task<void>::create([this, timeout](auto& resolve, auto& reject) {
      startOnce(timeout, resolve);
    });
  }
#endif

  void stop() {
    error::test(uv_timer_stop(*this));
  }

  void again() {
    error::test(uv_timer_again(*this));
  }

  void repeat(uint64_t repeat) noexcept {
    uv_timer_set_repeat(*this, repeat);
  }

  uint64_t repeat() const noexcept {
    return uv_timer_get_repeat(*this);
  }

private:
  uv_timer_t* _native_timer;
};

void timeout(uint64_t timeout, std::function<void()> cb) {
  uv::timer* timer = new uv::timer();
  timer->startOnce(timeout, [timer, cb{std::move(cb)}]() {
    delete timer;
    cb();
  });
}

#ifndef UVPP_NO_TASK
task<void> timeout(uint64_t timeout) {
  uv::timer timer;
  co_await timer.startOnce(timeout);
}
#endif
} // namespace uv
