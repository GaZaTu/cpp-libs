#pragma once

#ifndef UVPP_NO_TASK
#include "../task.hpp"
#endif
#include "uv.h"
#include <functional>

namespace uv {
struct handle {
public:
  struct data {
    std::function<void()> close_cb;

    virtual ~data() {
    }
  };

  handle(uv_handle_t* native_handle, data* data_ptr) : _native_handle(native_handle) {
#if (UV_VERSION_MAJOR >= 1) && (UV_VERSION_MINOR >= 34)
    uv_handle_set_data(native_handle, data_ptr);
#else
    native_handle->data = (void*)data_ptr;
#endif
  }

  template <typename T>
  handle(T* native_handle, data* data_ptr) : handle(reinterpret_cast<uv_handle_t*>(native_handle), data_ptr) {
  }

  handle() = delete;

  handle(const handle&) = delete;

  virtual ~handle() noexcept {
    data* data_ptr = getData<data>();

    if (isClosing()) {
      delete data_ptr;
    } else {
      close([data_ptr]() {
        delete data_ptr;
      });
    }
  }

  operator uv_handle_t*() noexcept {
    return _native_handle;
  }

  operator const uv_handle_t*() const noexcept {
    return _native_handle;
  }

  bool isActive() const noexcept {
    return uv_is_active(*this) != 0;
  }

  bool isClosing() const noexcept {
    return uv_is_closing(*this) != 0;
  }

  virtual void close(std::function<void()> close_cb) noexcept {
    data* data_ptr = getData<data>();
    data_ptr->close_cb = close_cb;

    uv_close(*this, [](uv_handle_t* native_handle) {
      data* data_ptr = handle::getData<data>(native_handle);
      data_ptr->close_cb();
    });
  }

#ifndef UVPP_NO_TASK
  task<void> close() {
    return task<void>::create([this](auto& resolve, auto& reject) {
      close([&resolve]() {
        resolve();
      });
    });
  }
#endif

protected:
  template <typename R, typename T>
  static R* getData(const T* native_handle) {
#if (UV_VERSION_MAJOR >= 1) && (UV_VERSION_MINOR >= 34)
    return reinterpret_cast<R*>(uv_handle_get_data(reinterpret_cast<const uv_handle_t*>(native_handle)));
#else
    return reinterpret_cast<R*>(reinterpret_cast<const uv_req_t*>(native_handle)->data);
#endif
  }

  template <typename R>
  R* getData() {
    return handle::getData<R>(_native_handle);
  }

private:
  uv_handle_t* _native_handle;
};
} // namespace uv
