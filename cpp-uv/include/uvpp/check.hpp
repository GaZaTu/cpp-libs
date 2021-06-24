#pragma once

#include "./error.hpp"
#include "./handle.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct check : public handle {
public:
  struct data : public handle::data {
    uv_check_t* _native_check;
    std::function<void()> check_cb;

    data(uv_check_t* native_check) : _native_check(native_check) {
    }

    virtual ~data() {
      delete _native_check;
    }
  };

  check(uv_loop_t* native_loop, uv_check_t* native_check)
      : handle(native_check, new data(native_check)), _native_check(native_check) {
    error::test(uv_check_init(native_loop, native_check));
  }

  check(uv_loop_t* native_loop) : check(native_loop, new uv_check_t()) {
  }

  check(uv_check_t* native_check) : check(uv_default_loop(), native_check) {
  }

  check() : check(uv_default_loop(), new uv_check_t()) {
  }

  check(check&& source) noexcept
      : handle(source._native_check, handle::getData<data>(source._native_check)),
        _native_check(std::exchange(source._native_check, nullptr)) {
  }

  operator uv_check_t*() noexcept {
    return _native_check;
  }

  operator const uv_check_t*() const noexcept {
    return _native_check;
  }

  void start(std::function<void()> check_cb) {
    data* data_ptr = getData<data>();
    data_ptr->check_cb = check_cb;

    error::test(uv_check_start(*this, [](uv_check_t* native_check) {
      data* data_ptr = handle::getData<data>(native_check);
      data_ptr->check_cb();
    }));
  }

  void stop() {
    error::test(uv_check_stop(*this));
  }

private:
  uv_check_t* _native_check;
};
} // namespace uv
