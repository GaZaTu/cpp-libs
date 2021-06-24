#pragma once

#include "./rx.hpp"
#include "./uv.hpp"

namespace uv {
rxcpp::observable<std::nullopt_t> checkStartAsObservable(uv::check& check) {
  static int i;

  return rxcpp::observable<>::create<std::nullopt_t>([&check](rxcpp::subscriber<std::nullopt_t> s) {
    check.start([s]() {
      s.on_next(i++);
    });

    return [&check]() {
      check.stop();
    };
  });
}

rxcpp::observable<std::nullopt_t> timerStartAsObservable(uv::timer& timer, uint64_t timeout, uint64_t repeat = 0) {
  return rxcpp::observable<>::create<std::nullopt_t>([&timer, timeout, repeat](rxcpp::subscriber<std::nullopt_t> s) {
    timer.start(
        [s]() {
          s.on_next(std::nullopt);
        },
        timeout, repeat);

    return [&timer]() {
      timer.stop();
    };
  });
}
} // namespace uv
