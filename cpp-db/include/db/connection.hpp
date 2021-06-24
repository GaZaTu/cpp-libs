#pragma once

#include "./datasource.hpp"
#include <memory>
#include <optional>
#include <string_view>

namespace db {
class connection {
public:
  friend statement;
  friend transaction;

  template <typename T>
  friend class orm::selector;

  template <typename T>
  friend class orm::deleter;

  template <typename T>
  friend class orm::updater;

  explicit connection(datasource& dsrc) : _dsrc(dsrc) {
    _datasource_connection = _dsrc.getConnection();

    if (_dsrc.onConnectionOpen()) {
      _dsrc.onConnectionOpen()(*this);
    }
  }

  connection(const connection&) = delete;

  connection(connection&&) = delete;

  connection& operator=(const connection&) = delete;

  connection& operator=(connection&&) = delete;

  ~connection() {
    if (_dsrc.onConnectionClose()) {
      _dsrc.onConnectionClose()(*this);
    }
  }

  void execute(const std::string_view script) {
    _datasource_connection->execute(script);
  }

  template <typename T>
  std::optional<T> execute(const std::string_view script);

  int version() {
    return _datasource_connection->getVersion();
  }

  void upgrade(int newVersion = -1) {
    int oldVersion = version();

    for (auto& update : _dsrc.updates()) {
      if (update.version <= oldVersion || (update.version > newVersion && newVersion != -1)) {
        continue;
      }

      update.up(*this);

      _datasource_connection->setVersion(update.version);
    }
  }

  void downgrade(int newVersion) {
    int oldVersion = version();

    for (int i = _dsrc.updates().size() - 1; i >= 0; i--) {
      auto& update = _dsrc.updates().at(i);

      if (update.version > oldVersion || update.version <= newVersion) {
        continue;
      }

      update.down(*this);

      _datasource_connection->setVersion(update.version);
    }
  }

  template <typename T = datasource::connection>
  std::shared_ptr<T> getNativeConnection() {
    return std::dynamic_pointer_cast<T>(_datasource_connection);
  }

private:
  datasource& _dsrc;
  std::shared_ptr<datasource::connection> _datasource_connection;
};
} // namespace db
