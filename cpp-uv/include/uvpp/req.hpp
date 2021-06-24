#pragma once

#include "./error.hpp"
#include "uv.h"
#include <functional>

namespace uv {
namespace detail {
struct req {
public:
  struct data {
    uv::detail::req* req;

    virtual ~data() noexcept {
    }
  };

  req(uv_req_t* native_req, data* data_ptr) : _native_req(native_req) {
#if (UV_VERSION_MAJOR >= 1) && (UV_VERSION_MINOR >= 34)
    uv_req_set_data(native_req, data_ptr);
#else
    native_req->data = (void*)data_ptr;
#endif
  }

  template <typename T>
  req(T* native_req, data* data_ptr) : req(reinterpret_cast<uv_req_t*>(native_req), data_ptr) {
  }

  req() = delete;

  req(const req&) = delete;

  virtual ~req() noexcept {
  }

  operator uv_req_t*() noexcept {
    return _native_req;
  }

  operator const uv_req_t*() const noexcept {
    return _native_req;
  }

  void cancel() {
    error::test(uv_cancel(*this));
  }

protected:
  template <typename R, typename T>
  static R* getData(const T* native_req) {
#if (UV_VERSION_MAJOR >= 1) && (UV_VERSION_MINOR >= 34)
    return reinterpret_cast<R*>(uv_req_get_data(reinterpret_cast<const uv_req_t*>(native_req)));
#else
    return reinterpret_cast<R*>(reinterpret_cast<const uv_req_t*>(native_req)->data);
#endif
  }

  template <typename R>
  R* getData() {
    return req::getData<R>(_native_req);
  }

private:
  uv_req_t* _native_req;
};
} // namespace detail

template <typename N, typename D>
struct req : public uv::detail::req {
public:
  using C = void (*)(N*);

  req(N* native, C cleanup) : uv::detail::req(native, new D()), _native(native), _cleanup(cleanup) {
    dataPtr()->req = this;
  }

  req(C cleanup) : req(new N(), cleanup) {
  }

  req() : req(new N(), nullptr) {
  }

  virtual ~req() noexcept {
    delete dataPtr();

    if (_cleanup) {
      _cleanup(_native);
    }

    delete _native;
  }

  operator N*() noexcept {
    return _native;
  }

  operator const N*() const noexcept {
    return _native;
  }

  D* dataPtr() {
    return getData<D>();
  }

  static D* dataPtr(N* native_fs) {
    return getData<D>(native_fs);
  }

private:
  N* _native;
  C _cleanup = nullptr;
};
} // namespace uv
