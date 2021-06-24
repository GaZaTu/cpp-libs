#pragma once

#include "./error.hpp"
#include "./req.hpp"
#ifndef UVPP_NO_TASK
#include "../task.hpp"
#endif
#include "finally.hpp"
#include "uv.h"
#include <functional>
#include <string_view>

namespace uv {
using file = uv_file;

namespace fs {
using buf = uv_buf_t;

void close(uv::file file, std::function<void()> cb, uv_loop_t* native_loop = uv_default_loop()) {
  struct data_t : public uv::detail::req::data {
    std::function<void()> cb;
  };
  using req_t = uv::req<uv_fs_t, data_t>;

  auto req = new req_t(&uv_fs_req_cleanup);
  auto data = req->dataPtr();
  data->cb = cb;

  error::test(uv_fs_close(native_loop, *req, file, [](uv_fs_t* req) {
    auto data = req_t::dataPtr(req);
    auto cb = std::move(data->cb);
    delete data->req;

    cb();
  }));
}

#ifndef UVPP_NO_TASK
task<void> close(uv::file file, uv_loop_t* native_loop = uv_default_loop()) {
  return task<void>::create([file, native_loop](auto& resolve, auto& reject) {
    uv::fs::close(
        file,
        [&resolve, &reject]() {
          resolve();
        },
        native_loop);
  });
}
#endif

void open(const char* path, int flags, int mode, std::function<void(uv::file, uv::error)> cb,
    uv_loop_t* native_loop = uv_default_loop()) {
  struct data_t : public uv::detail::req::data {
    std::function<void(uv::file, uv::error)> cb;
  };
  using req_t = uv::req<uv_fs_t, data_t>;

  auto req = new req_t(&uv_fs_req_cleanup);
  auto data = req->dataPtr();
  data->cb = cb;

  error::test(uv_fs_open(native_loop, *req, path, flags, mode, [](uv_fs_t* req) {
    auto data = req_t::dataPtr(req);
    auto cb = std::move(data->cb);
    delete data->req;

    if (req->result < 0) {
      cb(0, uv::error{(int)req->result});
    } else {
      cb(req->result, uv::error{0});
    }
  }));
}

#ifndef UVPP_NO_TASK
task<uv::file> open(const char* path, int flags, int mode, uv_loop_t* native_loop = uv_default_loop()) {
  return task<uv::file>::create([path = std::string{path}, flags, mode, native_loop](auto& resolve, auto& reject) {
    uv::fs::open(
        path.data(), flags, mode,
        [&resolve, &reject](auto result, auto error) {
          if (error) {
            reject(make_exception_ptr(error));
          } else {
            resolve(result);
          }
        },
        native_loop);
  });
}
#endif

void read(uv::file file, std::function<void(std::string_view, uv::error)> cb, char* buf = nullptr, size_t buf_len = 0,
    int64_t offset = 0, uv_loop_t* native_loop = uv_default_loop()) {
  struct data_t : public uv::detail::req::data {
    bool owns_buf;
    uv::fs::buf buf;
    std::function<void(std::string_view, uv::error)> cb;
  };
  using req_t = uv::req<uv_fs_t, data_t>;

  bool owns_buf = buf == nullptr;
  if (owns_buf) {
    if (buf_len == 0) {
      buf_len = 65536;
    }

    buf = new char[buf_len];
  }

  auto req = new req_t(&uv_fs_req_cleanup);
  auto data = req->dataPtr();
  data->owns_buf = owns_buf;
  data->buf = uv_buf_init(buf, buf_len);
  data->cb = cb;

  error::test(uv_fs_read(native_loop, *req, file, &data->buf, 1, offset, [](uv_fs_t* req) {
    auto data = req_t::dataPtr(req);
    auto cb = std::move(data->cb);
    auto owns_buf = data->owns_buf;
    auto buf = data->buf;
    auto result = req->result;
    delete data->req;

    if (result < 0) {
      cb(std::string_view{nullptr, 0}, uv::error{(int)result});
    } else {
      cb(std::string_view{buf.base, (size_t)result}, uv::error{0});
    }

    if (owns_buf) {
      delete buf.base;
    }
  }));
}

#ifndef UVPP_NO_TASK
task<std::string_view> read(uv::file file, char* buf = nullptr, size_t buf_len = 0, int64_t offset = 0,
    uv_loop_t* native_loop = uv_default_loop()) {
  return task<std::string_view>::create([file, buf, buf_len, offset, native_loop](auto& resolve, auto& reject) {
    uv::fs::read(
        file,
        [&resolve, &reject](auto result, auto error) {
          if (error) {
            reject(make_exception_ptr(error));
          } else {
            resolve(result);
          }
        },
        buf, buf_len, offset, native_loop);
  });
}
#endif

#ifndef UVPP_NO_TASK
task<std::string> readAll(uv::file file, int64_t offset = 0, uv_loop_t* native_loop = uv_default_loop()) {
  std::string result;

  char buf[65536];
  while (true) {
    auto chunk = co_await read(file, buf, sizeof(buf), offset + result.length(), native_loop);

    if (chunk.length() == 0) {
      break;
    }

    result += chunk;
  }

  co_return result;
}
#endif

#ifndef UVPP_NO_TASK
task<std::string> readAll(const char* path, int64_t offset = 0, uv_loop_t* native_loop = uv_default_loop()) {
  uv::file file = co_await uv::fs::open(path, O_RDONLY, S_IRUSR, native_loop);
  finally f{[file, native_loop]() {
    uv::fs::close(
        file,
        []() {
        },
        native_loop);
  }};

  co_return co_await uv::fs::readAll(file, offset, native_loop);
}
#endif
} // namespace fs
} // namespace uv
