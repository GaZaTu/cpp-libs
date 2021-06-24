#pragma once

#include "./error.hpp"
#include "./stream.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct tty : public stream {
public:
  enum file {
    STDIN = 0,
    STDOUT = 1,
    STDERR = 2,
  };

  struct data : public stream::data {
    uv_tty_t* _native_tty;

    data(uv_tty_t* native_tty) : _native_tty(native_tty) {
    }

    virtual ~data() {
      delete _native_tty;
    }
  };

  tty(file fd, uv_loop_t* native_loop, uv_tty_t* native_tty)
      : stream(native_tty, new data(native_tty)), _native_tty(native_tty) {
    error::test(uv_tty_init(native_loop, native_tty, fd, fd == STDIN));
  }

  tty(file fd, uv_loop_t* native_loop) : tty(fd, native_loop, new uv_tty_t()) {
  }

  tty(file fd, uv_tty_t* native_tty) : tty(fd, uv_default_loop(), native_tty) {
  }

  tty(file fd) : tty(fd, uv_default_loop(), new uv_tty_t()) {
  }

  tty(tty&& source) noexcept
      : stream(source._native_tty, handle::getData<data>(source._native_tty)),
        _native_tty(std::exchange(source._native_tty, nullptr)) {
  }

  operator uv_tty_t*() noexcept {
    return _native_tty;
  }

  operator const uv_tty_t*() const noexcept {
    return _native_tty;
  }

private:
  uv_tty_t* _native_tty;
};
} // namespace uv
