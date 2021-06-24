#pragma once

#include "uv.h"

namespace uv {
uint64_t hrtime() {
  return uv_hrtime();
}
} // namespace uv
