#pragma once

#include "uv.h"
#include <stdexcept>
#include <string>

namespace uv {
class error : public std::runtime_error {
public:
  int code;

  error(const std::string& msg) : std::runtime_error(msg) {
    this->code = -1;
  }

  error(int code = 0) : std::runtime_error(strerror(code)) {
    this->code = code;
  }

  operator bool() {
    return this->code != 0;
  }

  bool operator==(int code) {
    return this->code == code;
  }

  static void test(int code) {
    if (code != 0) {
      throw error(code);
    }
  }

private:
  static const char* strerror(int code) {
    if (code == -1) {
      return "unknown";
    }

    if (code == 0) {
      return "nothing";
    }

    return uv_strerror(code);
  }
};
} // namespace uv
