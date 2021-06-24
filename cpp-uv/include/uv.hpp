#pragma once

#include "./uvpp/async.hpp"
#include "./uvpp/check.hpp"
#include "./uvpp/dns.hpp"
#include "./uvpp/error.hpp"
#include "./uvpp/fs.hpp"
#include "./uvpp/handle.hpp"
#include "./uvpp/loop.hpp"
#include "./uvpp/misc.hpp"
#include "./uvpp/req.hpp"
#include "./uvpp/stream.hpp"
#include "./uvpp/tcp.hpp"
#include "./uvpp/threading.hpp"
#include "./uvpp/timer.hpp"
#include "./uvpp/tty.hpp"
#include "./uvpp/work.hpp"

#ifndef UVPP_NO_TASK
namespace uv {
void deleteTask(std::function<void()> deleter) {
  uv::async::queue(deleter);
}

void startTask(task<void>& task) {
  task.start(deleteTask);
}

template <typename F>
void startAsTask(F&& taskfn) {
  taskfn().start(deleteTask);
}
} // namespace uv
#endif
