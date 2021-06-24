#pragma once

#include "uv.h"

namespace uv {
void run() {
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
} // namespace uv
