#pragma once

#include "./error.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct thread {
public:
  struct data {
    std::function<void()> entry;

    data(std::function<void()>& e) : entry(std::move(e)) {
    }
  };

  thread(std::function<void()> entry) : _native(new uv_thread_t()) {
    error::test(uv_thread_create(
        _native,
        [](void* ptr) {
          auto data_ptr = reinterpret_cast<data*>(ptr);
          auto entry = std::move(data_ptr->entry);
          delete data_ptr;

          entry();
        },
        new data(entry)));
  }

  void join() {
    error::test(uv_thread_join(_native));
  }

  bool operator==(thread& other) {
    return uv_thread_equal(_native, other._native) != 0;
  }

private:
  uv_thread_t* _native;
};

struct rwlock {
public:
  struct read {
  public:
    read(rwlock& l) : _l(l) {
      uv_rwlock_rdlock(_l._native);
    }

    ~read() {
      uv_rwlock_rdunlock(_l._native);
    }

  private:
    rwlock& _l;
  };

  struct write {
  public:
    write(rwlock& l) : _l(l) {
      uv_rwlock_wrlock(_l._native);
    }

    ~write() {
      uv_rwlock_wrunlock(_l._native);
    }

  private:
    rwlock& _l;
  };

  friend read;
  friend write;

  rwlock() : _native(new uv_rwlock_t()) {
    error::test(uv_rwlock_init(_native));
  }

  ~rwlock() {
    uv_rwlock_destroy(_native);
    delete _native;
  }

private:
  uv_rwlock_t* _native;
};
} // namespace uv
