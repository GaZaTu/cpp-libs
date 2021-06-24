#pragma once

#include <coroutine>
#include <functional>
#include <optional>
#include <variant>
#include <iostream>

#include "cppcoro/async_manual_reset_event.hpp"

template <typename T = void>
struct [[nodiscard]] task {
  using value_type = std::conditional_t<std::is_same_v<T, void>, std::nullopt_t, T>;

  static constexpr bool is_void_v = std::is_same_v<value_type, std::nullopt_t>;
  static constexpr bool is_integral_v = std::is_integral_v<value_type>;
  static constexpr bool is_move_constructible_v = std::is_move_constructible_v<value_type>;
  static constexpr bool is_copy_constructible_v = std::is_copy_constructible_v<value_type>;
  static constexpr bool is_basic_type_v = std::is_arithmetic_v<value_type> || std::is_pointer_v<value_type>;

  static constexpr bool use_movable_param = is_move_constructible_v && !is_basic_type_v && !is_void_v;
  static constexpr bool use_const_param = is_copy_constructible_v && !is_move_constructible_v && !is_basic_type_v && !is_void_v;
  static constexpr bool use_value_param = ((!is_move_constructible_v && !is_copy_constructible_v) || is_basic_type_v) && !is_void_v;

  struct promise_type_base {
    std::variant<std::monostate, value_type, std::exception_ptr> result;
    std::coroutine_handle<> waiter; // who waits on this coroutine

    void unhandled_exception(std::exception_ptr error) {
      result.template emplace<2>(error);
    }

    void unhandled_exception() {
      unhandled_exception(std::current_exception());
    }

    std::suspend_always initial_suspend() {
      return {};
    }

    auto final_suspend() noexcept {
      struct final_awaiter {
        bool await_ready() noexcept {
          return false;
        }

        void await_resume() noexcept {
        }

        auto await_suspend(std::coroutine_handle<promise_type> me) noexcept {
          return me.promise().waiter;
        }
      };

      return final_awaiter{};
    }
  };

  struct promise_type_movable_param : public promise_type_base {
    auto get_return_object() {
      return task{*this};
    }

    void return_value(value_type&& value) {
      this->result.template emplace<1>(std::move(value));
    }

    void return_value(value_type& value) {
      this->result.template emplace<1>(std::move(value));
    }

    value_type&& unpack() {
      if (this->result.index() == 2) {
        std::rethrow_exception(std::get<2>(this->result));
      }

      return std::move(std::get<1>(this->result));
    }
  };

  struct promise_type_const_param : public promise_type_base {
    auto get_return_object() {
      return task{*this};
    }

    void return_value(const value_type& value) {
      this->result.template emplace<1>(value);
    }

    value_type unpack() {
      if (this->result.index() == 2) {
        std::rethrow_exception(std::get<2>(this->result));
      }

      return std::get<1>(this->result);
    }
  };

  struct promise_type_value_param : public promise_type_base {
    auto get_return_object() {
      return task{*this};
    }

    void return_value(value_type value) {
      this->result.template emplace<1>(value);
    }

    value_type unpack() {
      if (this->result.index() == 2) {
        std::rethrow_exception(std::get<2>(this->result));
      }

      return std::get<1>(this->result);
    }
  };

  struct promise_type_void_param : public promise_type_base {
    auto get_return_object() {
      return task{*this};
    }

    void return_void() {
    }

    value_type unpack() {
      if (this->result.index() == 2) {
        std::rethrow_exception(std::get<2>(this->result));
      }

      return std::nullopt;
    }
  };

  using promise_type = std::conditional_t<is_void_v, promise_type_void_param,
      std::conditional_t<use_movable_param, promise_type_movable_param,
          std::conditional_t<use_const_param, promise_type_const_param,
              std::conditional_t<use_value_param, promise_type_value_param, void>>>>;

  task() : _handle(nullptr) {}

  task(task&& rhs) : _handle(rhs._handle) {
    rhs._handle = nullptr;
  }

  task& operator=(task&& rhs) {
    if (_handle) {
      _handle.destroy();
    }

    _handle = rhs._handle;
    rhs._handle = nullptr;

    return *this;
  }

  task(const task&) = delete;
  
  task& operator=(const task&) = delete;

  ~task() {
    if (_handle) {
      _handle.destroy();
    }
  }

  explicit task(std::coroutine_handle<promise_type>& handle) : _handle(std::move(handle)) {
  }

  explicit task(promise_type& promise) : _handle(std::coroutine_handle<promise_type>::from_promise(promise)) {
  }

  bool await_ready() {
    return false; // no idea why i return false here
  }

  auto await_resume() {
    return unpack();
  }

  void await_suspend(std::coroutine_handle<> waiter) {
    if constexpr (is_void_v) {
      if (!_handle) {
        waiter.resume();
        return;
      }
    }

    _handle.promise().waiter = waiter;
    _handle.resume();
  }

  void start() {
    await_suspend(std::noop_coroutine());
  }

  void start(std::function<void(std::function<void()>)> queue_delete) {
    struct state_t {
      std::function<void(std::function<void()>)> queue_delete;
      task<T> self;
      task<void> deleter;

      static task<void> await_and_delete(state_t* state) {
        try {
          co_await state->self;
        } catch (const std::exception& error) {
          std::cerr << "unhandled task exception: " << typeid(error).name() << ": " << error.what() << std::endl;
        } catch (...) {
          std::cerr << "unhandled task error" << std::endl;
        }

        state->queue_delete([state]() {
          delete state;
        });
      }
    };

    auto state = new state_t();
    state->queue_delete = std::move(queue_delete);
    state->self = std::move(*this);
    state->deleter = state_t::await_and_delete(state);

    state->deleter.start();
  }

  bool done() {
    if constexpr (is_void_v) {
      if (!_handle) {
        return true;
      }
    }

    return _handle.done();
  }

  auto unpack() {
    if constexpr (is_void_v) {
      if (!_handle) {
        return std::nullopt;
      }
    }

    return _handle.promise().unpack();
  }

  using resolver_movable_param = std::function<void(value_type&)>;
  using resolver_const_param = std::function<void(const value_type&)>;
  using resolver_value_param = std::function<void(value_type)>;
  using resolver_void_param = std::function<void()>;

  using resolver = std::conditional_t<is_void_v, resolver_void_param,
      std::conditional_t<use_movable_param, resolver_movable_param,
          std::conditional_t<use_const_param, resolver_const_param,
              std::conditional_t<use_value_param, resolver_value_param, void>>>>;

  using rejecter = std::function<void(std::exception_ptr)>;

  static task<T> create(std::function<void(resolver&, rejecter&)> cb) {
    cppcoro::async_manual_reset_event event;
    promise_type promise;

    resolver resolve = create_resolver(event, promise);
    rejecter reject = create_rejecter(event, promise);

    try {
      cb(resolve, reject);
    } catch (...) {
      reject(std::current_exception());
    }

    co_await event;

    if constexpr (std::is_same_v<promise_type, promise_type_void_param>) {
      promise.unpack();
    } else {
      co_return promise.unpack();
    }
  }

  using resolved_movable_param = value_type&;
  using resolved_const_param = const value_type&;
  using resolved_value_param = value_type;
  using resolved_void_param = std::nullopt_t;

  using resolved = std::conditional_t<is_void_v, resolved_void_param,
      std::conditional_t<use_movable_param, resolved_movable_param,
          std::conditional_t<use_const_param, resolved_const_param,
              std::conditional_t<use_value_param, resolved_value_param, void>>>>;

  static task<T> resolve(resolved value) {
    return create([&](auto& resolve, auto&) {
      if constexpr (is_void_v) {
        resolve();
      } else {
        resolve(value);
      }
    });
  }

  static task<T> reject(std::exception_ptr error) {
    return create([&](auto&, auto& reject) {
      reject(error);
    });
  }

  static task<T> reject(const std::exception& error) {
    return create([&](auto&, auto& reject) {
      reject(std::make_exception_ptr(error));
    });
  }

  task<void> then(resolver then, rejecter fail) {
    try {
      then(co_await *this);
    } catch (...) {
      fail(std::current_exception());
    }
  }

  task<void> finally(rejecter cb) {
    try {
      co_await *this;
      cb(nullptr);
    } catch (...) {
      cb(std::current_exception());
    }
  }

  template <typename F>
  static void race(std::vector<task<void>>& vector, F& cb) {}

  template <typename F, typename A0, typename... A>
  static void race(std::vector<task<void>>& vector, F& cb, A0 task, A... tasks) {
    vector.emplace_back(task.finally(cb));
    race(vector, cb, tasks...);
  }

  template <typename... A>
  static task<void> race(A... tasks) {
    std::vector<task<void>> vector;
    cppcoro::async_manual_reset_event event;
    std::exception_ptr error = nullptr;

    auto cb = [&](std::exception_ptr e) {
      error = e;
      event.set();
    };

    race(vector, cb, tasks...);

    co_await event;

    if (error) {
      std::rethrow_exception(error);
    }
  }

private:
  std::coroutine_handle<promise_type> _handle;

  static resolver create_resolver(cppcoro::async_manual_reset_event& event, promise_type& promise) {
    if constexpr (is_void_v) {
      return [&]() {
        promise.return_void();
        event.set();
      };
    } else if constexpr (use_movable_param) {
      return [&](value_type& value) {
        promise.return_value(std::move(value));
        event.set();
      };
    } else if constexpr (use_const_param) {
      return [&](const value_type& value) {
        promise.return_value(value);
        event.set();
      };
    } else if constexpr (use_value_param) {
      return [&](value_type value) {
        promise.return_value(value);
        event.set();
      };
    }
  }

  static rejecter create_rejecter(cppcoro::async_manual_reset_event& event, promise_type& promise) {
    return [&](std::exception_ptr error) {
      promise.unhandled_exception(error);
      event.set();
    };
  }
};
