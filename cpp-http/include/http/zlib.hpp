#pragma once

#include "zlib.h"
#include <string>

namespace http {
int compress(std::string& _body) {
  std::string body;

  z_stream zstream;
  zstream.zalloc = nullptr;
  zstream.zfree = nullptr;
  zstream.opaque = nullptr;
  zstream.next_in = nullptr;
  zstream.avail_in = 0;
  deflateInit2(&zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);

  zstream.next_in = (Bytef*)_body.data();
  zstream.avail_in = (uInt)_body.length();

  char buffer[65536];
  while (true) {
    zstream.next_out = (Bytef*)&buffer;
    zstream.avail_out = (uInt)sizeof(buffer);

    int rc = deflate(&zstream, Z_FINISH);
    if (rc == Z_OK || rc == Z_STREAM_END || rc == Z_BUF_ERROR) {
      zstream.next_in += zstream.total_in;

      body += std::string_view{(const char*)&buffer, (size_t)zstream.total_out};

      if (rc == Z_STREAM_END) {
        _body = std::move(body);

        deflateEnd(&zstream);
        break;
      }
    } else {
      deflateEnd(&zstream);
      return rc;
    }
  }

  return 0;
}

int uncompress(std::string& _body) {
  std::string body;

  z_stream zstream;
  zstream.zalloc = nullptr;
  zstream.zfree = nullptr;
  zstream.opaque = nullptr;
  zstream.next_in = nullptr;
  zstream.avail_in = 0;
  inflateInit2(&zstream, 16 + MAX_WBITS);

  zstream.next_in = (Bytef*)_body.data();
  zstream.avail_in = (uInt)_body.length();

  char buffer[65536];
  while (true) {
    zstream.next_out = (Bytef*)&buffer;
    zstream.avail_out = (uInt)sizeof(buffer);

    int rc = inflate(&zstream, Z_NO_FLUSH);
    if (rc == Z_OK || rc == Z_STREAM_END || rc == Z_BUF_ERROR) {
      zstream.next_in += zstream.total_in;

      body += std::string_view{(const char*)&buffer, (size_t)zstream.total_out};

      if (rc != Z_BUF_ERROR) {
        _body = std::move(body);

        inflateEnd(&zstream);
        break;
      }
    } else {
      inflateEnd(&zstream);
      return rc;
    }
  }

  return 0;
}
} // namespace http
