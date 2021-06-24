#pragma once

#include "../uvpp/async.hpp"
#include "../uvpp/tcp.hpp"
#include "./http1.hpp"
#include "./http2.hpp"
#ifndef UVPP_NO_SSL
#include "../ssl-openssl.hpp"
#endif

namespace http {
task<http::response> fetch(http::request& request) {
  request.headers["host"] = request.url.host;
  request.headers["connection"] = "close";
  request.headers["accept-encoding"] = "gzip";

#ifndef UVPP_NO_SSL
  ssl::openssl::driver opensslDriver;
  ssl::context sslContext{opensslDriver, ssl::CONNECT};
  sslContext.useALPNProtocols({"h2", "http/1.1"});
#endif

  uv::tcp tcp;

  if (request.url.schema == "https") {
#ifndef UVPP_NO_SSL
    tcp.useSSL(sslContext);
#endif
  }

  co_await tcp.connect(request.url.host, request.url.port);

  if (tcp.sslState().protocol() == "h2") {
    http2::handler<http::response> handler;
    handler.onSend([&](auto input) {
      tcp.write((std::string)input, [](auto) {
      });
    });
    handler.complete([&](auto&) {
      uv::async::queue([&]() {
        tcp.readStop();
      });
    });

    handler.submitSettings();
    handler.submitRequest(request);
    handler.sendSession();

    co_await tcp.readStartUntilEOF([&](auto chunk) {
      handler.execute(chunk);
    });

    if (!handler) {
      throw http::error{"unexpected EOF"};
    }

    co_return handler.result();
  } else {
    http::parser<http::response> parser;

    co_await tcp.write((std::string)request);
    co_await tcp.shutdown();

    co_await tcp.readStartUntilEOF([&](auto chunk) {
      parser.execute(chunk);
    });

    if (!parser) {
      throw http::error{"unexpected EOF"};
    }

    co_return parser.result();
  }
}

task<http::response> fetch(http_method m, http::url u, std::string b = {}) {
  http::request request{m, u, b};
  co_return co_await fetch(request);
}

task<http::response> fetch(http::url u) {
  http::request request{u};
  co_return co_await fetch(request);
}
} // namespace http
