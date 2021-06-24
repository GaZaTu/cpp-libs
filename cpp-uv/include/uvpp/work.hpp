#pragma once

#include "./error.hpp"
#include "./req.hpp"
#ifndef UVPP_NO_TASK
#include "../task.hpp"
#endif
#include "uv.h"
#include <functional>
#include <variant>

namespace uv {
namespace work {
template <typename T>
void queue(std::function<T()> work_cb, std::function<void(std::optional<T>&&, std::exception_ptr)> after_work_cb,
    uv_loop_t* native_loop = uv_default_loop()) {
  struct data_t : public uv::detail::req::data {
    std::variant<std::monostate, T, std::exception_ptr> result;

    std::function<T()> work_cb;
    std::function<void(std::optional<T>&&, std::exception_ptr)> after_work_cb;
  };
  using req_t = uv::req<uv_work_t, data_t>;

  auto req = new req_t();
  auto data = req->dataPtr();
  data->work_cb = work_cb;
  data->after_work_cb = after_work_cb;

  error::test(uv_queue_work(
      native_loop, *req,
      [](uv_work_t* req) {
        auto data = req_t::dataPtr(req);

        try {
          data->result.template emplace<1>(std::move(data->work_cb()));
        } catch (...) {
          data->result.template emplace<2>(std::current_exception());
        }
      },
      [](uv_work_t* req, int status) {
        auto data = req_t::dataPtr(req);
        auto after_work_cb = std::move(data->after_work_cb);
        auto result = std::move(data->result);
        delete data->req;

        if (result.index() == 1) {
          after_work_cb(std::get<1>(result), nullptr);
        } else {
          after_work_cb(std::nullopt, std::get<2>(result));
        }
      }));
}

#ifndef UVPP_NO_TASK
template <typename T>
task<T> queue(std::function<T()> work_cb, uv_loop_t* native_loop = uv_default_loop()) {
  return task<T>::create([work_cb, native_loop](auto& resolve, auto& reject) {
    uv::work::queue<T>(
        work_cb,
        [&resolve, &reject](auto&& result, auto error) {
          if (error) {
            reject(error);
          } else {
            resolve(result.value());
          }
        },
        native_loop);
  });
}
#endif
} // namespace work
} // namespace uv
