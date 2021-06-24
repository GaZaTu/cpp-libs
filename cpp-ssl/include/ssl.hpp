#pragma once

#include <functional>
#include <memory>
#include <string_view>
#ifndef SSLPP_NO_TASK
#include "task.hpp"
#endif

namespace ssl {
enum method {
  TLS1_2,
};

enum filetype {
  PEM,
};

enum mode {
  CONNECT,
  ACCEPT,
};

class ssl_error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

struct driver {
public:
  struct state {
  public:
    virtual ~state() {
    }

    virtual void handshake(std::function<void()>& on_handshake) = 0;

    virtual bool ready() = 0;

    virtual void decrypt(std::string_view data) = 0;

    virtual void encrypt(std::string_view data, std::function<void(std::exception_ptr)> cb) = 0;

    virtual void onReadDecrypted(std::function<void(std::string_view)>& value) = 0;

    virtual void onWriteEncrypted(
        std::function<void(std::string&&, std::function<void(std::exception_ptr)>)>& value) = 0;

    virtual std::string_view protocol() = 0;
  };

  struct context {
  public:
    virtual ~context() {
    }

    virtual std::shared_ptr<state> createState() = 0;

    virtual void useCertificateFile(const char* path, ssl::filetype type) = 0;

    virtual void usePrivateKeyFile(const char* path, ssl::filetype type) = 0;

    virtual void useCertificateChainFile(const char* path) = 0;

    virtual void useALPNProtocols(const std::vector<std::string>& protocols) = 0;

    virtual void useALPNCallback(std::function<bool(std::string_view)>& cb) = 0;
  };

  virtual std::shared_ptr<context> getContext(ssl::mode mode) const = 0;
};

struct state;

struct context {
public:
  friend state;

  explicit context(ssl::driver& driver, ssl::mode mode = CONNECT) {
    _driver_context = driver.getContext(mode);
  }

  context(const context&) = delete;

  context(context&&) = delete;

  context& operator=(const context&) = delete;

  context& operator=(context&&) = delete;

  void useCertificateFile(const char* path, ssl::filetype type = ssl::PEM) {
    _driver_context->useCertificateFile(path, type);
  }

  void usePrivateKeyFile(const char* path, ssl::filetype type = ssl::PEM) {
    _driver_context->usePrivateKeyFile(path, type);
  }

  void useCertificateChainFile(const char* path) {
    _driver_context->useCertificateChainFile(path);
  }

  void useALPNProtocols(const std::vector<std::string>& protocols) {
    _driver_context->useALPNProtocols(protocols);
  }

  void useALPNCallback(std::function<bool(std::string_view)> cb) {
    _driver_context->useALPNCallback(cb);
  }

  void useALPNCallback(std::vector<std::string> protocols) {
    useALPNCallback([protocols](auto p) {
      for (const auto& protocol : protocols) {
        if (protocol == p) {
          return true;
        }
      }

      return false;
    });
  }

private:
  std::shared_ptr<ssl::driver::context> _driver_context;
};

struct state {
public:
  state() {
  }

  explicit state(ssl::context& context) {
    _driver_state = context._driver_context.get()->createState();
  }

  state(const state& other) {
    *this = other;
  }

  state& operator=(const state& other) {
    _driver_state = other._driver_state;
    return *this;
  };

  state(state&& other) {
    *this = other;
  }

  state& operator=(state&& other) {
    _driver_state = std::move(other._driver_state);
    return *this;
  }

  void handshake(std::function<void()> on_handshake) {
    _driver_state->handshake(on_handshake);
  }

#ifndef SSLPP_NO_TASK
  task<void> handshake() {
    return task<void>::create([this](auto& resolve, auto& reject) {
      handshake([&resolve, &reject]() {
        resolve();
      });
    });
  }
#endif

  bool ready() {
    return _driver_state->ready();
  }

  void decrypt(std::string_view data) {
    _driver_state->decrypt(data);
  }

  void encrypt(std::string_view data, std::function<void(std::exception_ptr)> cb) {
    _driver_state->encrypt(data, cb);
  }

#ifndef SSLPP_NO_TASK
  task<void> encrypt(std::string_view data) {
    return task<void>::create([this, data](auto& resolve, auto& reject) {
      encrypt(data, [&resolve, &reject](auto error) {
        if (error) {
          reject(error);
        } else {
          resolve();
        }
      });
    });
  }
#endif

  void onReadDecrypted(std::function<void(std::string_view)> value) {
    _driver_state->onReadDecrypted(value);
  }

  void onWriteEncrypted(std::function<void(std::string&&, std::function<void(std::exception_ptr)>)> value) {
    _driver_state->onWriteEncrypted(value);
  }

  std::string_view protocol() {
    if (!*this) {
      return {};
    }

    return _driver_state->protocol();
  }

  operator bool() {
    return _driver_state != nullptr;
  }

private:
  std::shared_ptr<ssl::driver::state> _driver_state;
};
} // namespace ssl
