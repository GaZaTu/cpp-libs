#pragma once

#include "./connection.hpp"
#include <shared_mutex>

namespace db::pooled {
class datasource : public db::datasource {
public:
  datasource(db::datasource& dsrc) : _dsrc(dsrc) {
  }

  std::shared_ptr<db::datasource::connection> getConnection() override {
#ifdef CMAKE_ENABLE_THREADING
    std::shared_lock rlock(_connections_mutex);
#endif

    for (auto& connection : _connections) {
      if (connection.use_count() > 1) {
        continue;
      }

      return connection;
    }

#ifdef CMAKE_ENABLE_THREADING
    rlock.unlock();
    std::unique_lock wlock(_connections_mutex);
#endif

    auto connection = _dsrc.getConnection();
    _connections.push_back(connection);

    return connection;
  }

  std::function<void(db::connection&)>& onConnectionOpen() override {
    return _dsrc.onConnectionOpen();
  }

  std::function<void(db::connection&)>& onConnectionClose() override {
    return _dsrc.onConnectionClose();
  }

  std::vector<db::orm::update>& updates() override {
    return _dsrc.updates();
  }

private:
  db::datasource& _dsrc;

  std::vector<std::shared_ptr<db::datasource::connection>> _connections;
#ifdef CMAKE_ENABLE_THREADING
  std::shared_mutex _connections_mutex;
#endif
};
} // namespace db::pooled
