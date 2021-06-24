#pragma once

#include "ssl.hpp"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

namespace ssl::openssl {
bool initialized = false;

class openssl_error : public ssl::ssl_error {
public:
  openssl_error(const std::string& msg) : ssl::ssl_error(msg) {
  }

  openssl_error(int code) : ssl::ssl_error(ERR_error_string(code, nullptr)) {
  }

  static void test(int code) {
    if (code != 0) {
      throw openssl_error(code);
    }
  }
};

struct driver : public ssl::driver {
public:
  struct shared {
    std::string _alpn_protocols;
    std::function<bool(std::string_view)> _alpn_callback;
  };

  struct state : public ssl::driver::state {
  public:
    state(SSL_CTX* native_context, ssl::mode mode) : _native_context(native_context) {
      _mode = mode;
      _native_state = SSL_new(_native_context);

      switch (mode) {
      case ssl::CONNECT:
        SSL_set_connect_state(_native_state);
        break;
      case ssl::ACCEPT:
        SSL_set_accept_state(_native_state);
        break;
      }

      _read = BIO_new(BIO_s_mem());
      BIO_set_nbio(_read, true);

      _write = BIO_new(BIO_s_mem());
      BIO_set_nbio(_write, true);

      SSL_set_bio(_native_state, _read, _write);
    }

    virtual ~state() override {
      SSL_free(_native_state); // frees BIOs
    }

    void handshake(std::function<void()>& on_handshake) override {
      _on_handshake = std::move(on_handshake);

      handshake();
    }

    bool ready() override {
      return SSL_is_init_finished(_native_state) != 0;
    }

    void decrypt(std::string_view data) override {
      BIO_write(_read, data.data(), data.length()); // TODO: handle result

      if (!ready()) {
        handshake();
        return;
      }

      char buffer[65536];
      while (true) {
        int rc = SSL_read(_native_state, (void*)&buffer, sizeof(buffer));
        if (rc < 0) {
          int error = getError(rc);
          if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
            throw openssl_error(error);
          }

          sendPending();

          break;
        }

        if (rc == 0) {
          break;
        }

        _on_read_decrypted(std::string_view{(const char*)&buffer, (size_t)rc});
      }
    }

    void encrypt(std::string_view data, std::function<void(std::exception_ptr)> cb) override {
      if (!ready()) {
        throw openssl_error("ssl_is_init_finished = 0");
      }

      while (true) {
        int rc = SSL_write(_native_state, data.data(), data.length());
        int error = getError(rc);

        if (rc > 0) {
          sendPending(cb);

          if (rc < data.length()) {
            data = std::string_view{data.data() + rc, data.length() - rc};
          } else {
            break;
          }
        }

        if (error != 0) {
          throw openssl_error(rc);
        }

        if (rc == 0) {
          break;
        }
      }
    }

    void onReadDecrypted(std::function<void(std::string_view)>& value) override {
      _on_read_decrypted = std::move(value);
    }

    void onWriteEncrypted(std::function<void(std::string&&, std::function<void(std::exception_ptr)>)>& value) override {
      _on_write_encrypted = std::move(value);
    }

    std::string_view protocol() override {
      auto _shared = (shared*)SSL_CTX_get_app_data(_native_context);

      if (_mode == CONNECT) {
        auto ptr = (const unsigned char*)_shared->_alpn_protocols.data();
        auto len = (unsigned int)_shared->_alpn_protocols.length();
        SSL_get0_alpn_selected(_native_state, &ptr, &len);

        return {(const char*)ptr, (size_t)len};
      } else {
        return _shared->_alpn_protocols;
      }
    }

  private:
    ssl::mode _mode;

    SSL_CTX* _native_context;
    SSL* _native_state;
    BIO* _read;
    BIO* _write;

    std::function<void()> _on_handshake;
    bool _on_handshake_called = false;

    std::function<void(std::string_view)> _on_read_decrypted;
    std::function<void(std::string&&, std::function<void(std::exception_ptr)>)> _on_write_encrypted;

    int getError(int rc) {
      return SSL_get_error(_native_state, rc);
    }

    void sendPending(std::function<void(std::exception_ptr)> cb = [](auto) {
    }) {
      int rc = BIO_pending(_write);
      if (rc > 0) {
        std::string encrypted;
        encrypted.reserve(rc);
        encrypted.resize(rc);
        rc = BIO_read(_write, encrypted.data(), encrypted.capacity());

        _on_write_encrypted(std::move(encrypted), std::move(cb));
      }
    }

    void handshake() {
      int rc = SSL_do_handshake(_native_state);
      int error = getError(rc);

      if (error == SSL_ERROR_NONE) {
        if (!_on_handshake_called && ready()) {
          _on_handshake_called = true;
          _on_handshake();
        }

        return;
      }

      if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
        ERR_print_errors_fp(stderr);

        if (_mode != ssl::ACCEPT) {
          throw openssl_error(error);
        }
      }

      sendPending();
    }
  };

  struct context : public ssl::driver::context {
  public:
    context(ssl::mode mode) : ssl::driver::context() {
      if (!ssl::openssl::initialized) {
        ssl::openssl::initialized = true;

        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
        ERR_load_crypto_strings();
        ERR_load_BIO_strings();
      }

      _mode = mode;
      switch (_mode) {
      case CONNECT:
        _native_context = SSL_CTX_new(TLS_client_method());
        break;
      case ACCEPT:
        _native_context = SSL_CTX_new(TLS_server_method());
        break;
      }

      SSL_CTX_set_app_data(_native_context, &_shared);
      SSL_CTX_set_options(_native_context, SSL_OP_ALL | NO_OLD_PROTOCOLS);
      SSL_CTX_set_min_proto_version(_native_context, TLS1_2_VERSION);

      // SSL_CTX_set_verify(_native_context, SSL_VERIFY_NONE, nullptr);
    }

    virtual ~context() override {
      SSL_CTX_free(_native_context);
    }

    std::shared_ptr<ssl::driver::state> createState() override {
      return std::make_shared<state>(_native_context, _mode);
    }

    void useCertificateFile(const char* path, ssl::filetype type) override {
      int error = SSL_CTX_use_certificate_file(_native_context, path, mapFiletype(type));
      if (error != 1) {
        throw openssl_error(error);
      }

      validateCertificateAndPrivateKey();
    }

    void usePrivateKeyFile(const char* path, ssl::filetype type) override {
      int error = SSL_CTX_use_PrivateKey_file(_native_context, path, mapFiletype(type));
      if (error != 1) {
        throw openssl_error(error);
      }

      validateCertificateAndPrivateKey();
    }

    void useCertificateChainFile(const char* path) override {
      int error = SSL_CTX_use_certificate_chain_file(_native_context, path);
      if (error != 1) {
        throw openssl_error(error);
      }
    }

    void useALPNProtocols(const std::vector<std::string>& protocols) override {
      for (const auto& protocol : protocols) {
        _shared._alpn_protocols += (unsigned char)protocol.length();
        _shared._alpn_protocols += protocol;
      }

      auto ptr = (const unsigned char*)_shared._alpn_protocols.data();
      auto len = (unsigned int)_shared._alpn_protocols.length();

      SSL_CTX_set_alpn_protos(_native_context, ptr, len);
    }

    void useALPNCallback(std::function<bool(std::string_view)>& cb) override {
      _shared._alpn_callback = std::move(cb);

      SSL_CTX_set_alpn_select_cb(
          _native_context,
          [](SSL* ssl, const unsigned char** out, unsigned char* outlen, const unsigned char* in, unsigned int inlen,
              void* arg) {
            auto context = (ssl::openssl::driver::context*)arg;

            for (size_t i = 0; i < inlen; i++) {
              size_t offset = i + 1;
              size_t len = (size_t)in[i];

              std::string_view view{(const char*)(in + offset), len};
              if (context->_shared._alpn_callback(view)) {
                *out = in + offset;
                *outlen = len;

                context->_shared._alpn_protocols = view;

                return SSL_TLSEXT_ERR_OK;
              } else {
                i += len;
              }
            }

            return SSL_TLSEXT_ERR_ALERT_FATAL;
          },
          this);
    }

  private:
    static constexpr int NO_SSL = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    static constexpr int NO_TLS = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1; // | SSL_OP_NO_TLSv1_2
    static constexpr int NO_OLD_PROTOCOLS = NO_SSL | NO_TLS;

    ssl::mode _mode;

    SSL_CTX* _native_context;

    shared _shared;

    int _certkey_count = 0;

    void validateCertificateAndPrivateKey() {
      if (++_certkey_count == 2) {
        if (!SSL_CTX_check_private_key(_native_context)) {
          throw openssl_error("'certificate' and 'private key' do not match");
        }
      }
    }

    static int mapFiletype(ssl::filetype type) {
      switch (type) {
      case ssl::PEM:
        return SSL_FILETYPE_PEM;
      default:
        return 0;
      }
    }
  };

  std::shared_ptr<ssl::driver::context> getContext(ssl::mode mode) const override {
    return std::make_shared<context>(mode);
  }

private:
};
} // namespace ssl::openssl
