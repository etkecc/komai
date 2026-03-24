// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/api/CacheApiSignals.h"
#include "cache/api/CacheApiContext.h"
#include "cache/core/Cache_p.h"

#include <memory>
#include <utility>
#include <vector>

#include <QObject>
#include <QPointer>

namespace cache {

namespace {
struct PendingCacheCallback
{
    QPointer<QObject> receiver;
    std::function<void()> callback;
};

std::vector<PendingCacheCallback> &
pendingCallbacks()
{
    static std::vector<PendingCacheCallback> instance;
    return instance;
}

void
connectWhenCacheAvailable(QObject *receiver, std::function<void()> callback)
{
    if (!receiver)
        return;

    if (cacheInstance()) {
        callback();
        return;
    }

    pendingCallbacks().push_back({QPointer<QObject>(receiver), std::move(callback)});
}
} // namespace

void
drainPendingCacheCallbacks()
{
    auto pending = std::move(pendingCallbacks());
    pendingCallbacks().clear();

    for (auto &entry : pending) {
        if (entry.receiver)
            entry.callback();
    }
}

void
onReadReceiptsChanged(QObject *receiver, std::function<void()> callback)
{
    connectWhenCacheAvailable(receiver, [receiver, callback = std::move(callback)]() {
        QObject::connect(cacheInstance().get(),
                         &MatrixStore::newReadReceipts,
                         receiver,
                         [callback = std::move(callback)](
                           const QString &, const std::vector<QString> &) { callback(); });
    });
}

void
onReadReceiptsChanged(QObject *receiver,
                      std::function<void(const QString &, const std::vector<QString> &)> callback)
{
    connectWhenCacheAvailable(receiver, [receiver, callback = std::move(callback)]() {
        QObject::connect(cacheInstance().get(),
                         &MatrixStore::newReadReceipts,
                         receiver,
                         [callback = std::move(callback)](const QString &room_id,
                                                          const std::vector<QString> &event_ids) {
                             callback(room_id, event_ids);
                         });
    });
}

void
onRoomReadStatusChanged(QObject *receiver,
                        std::function<void(const std::map<QString, bool> &)> callback)
{
    connectWhenCacheAvailable(receiver, [receiver, callback = std::move(callback)]() {
        QObject::connect(cacheInstance().get(),
                         &MatrixStore::roomReadStatus,
                         receiver,
                         [callback = std::move(callback)](const std::map<QString, bool> &status) {
                             callback(status);
                         });
    });
}

void
disconnectFromCache(QObject *receiver)
{
    if (!cacheInstance())
        return;
    QObject::disconnect(cacheInstance().get(), nullptr, receiver, nullptr);
}

void
onDatabaseReady(QObject *receiver, std::function<void()> callback)
{
    connectWhenCacheAvailable(receiver, [receiver, callback = std::move(callback)]() {
        if (cacheInstance()->isDatabaseReady()) {
            callback();
            return;
        }

        QObject::connect(cacheInstance().get(),
                         &MatrixStore::databaseReady,
                         receiver,
                         [callback = std::move(callback)]() { callback(); });
    });
}

void
onSecretChanged(QObject *receiver, std::function<void(const std::string &)> callback)
{
    connectWhenCacheAvailable(receiver, [receiver, callback = std::move(callback)]() {
        QObject::connect(
          cacheInstance().get(),
          &MatrixStore::secretChanged,
          receiver,
          [callback = std::move(callback)](const std::string &name) { callback(name); });
    });
}

void
onVerificationStatusChanged(QObject *receiver, std::function<void(const std::string &)> callback)
{
    connectWhenCacheAvailable(receiver, [receiver, callback = std::move(callback)]() {
        QObject::connect(
          cacheInstance().get(),
          &MatrixStore::verificationStatusChanged,
          receiver,
          [callback = std::move(callback)](const std::string &user_id) { callback(user_id); });
    });
}

void
onSelfVerificationStatusChanged(QObject *receiver, std::function<void()> callback)
{
    connectWhenCacheAvailable(receiver, [receiver, callback = std::move(callback)]() {
        QObject::connect(cacheInstance().get(),
                         &MatrixStore::selfVerificationStatusChanged,
                         receiver,
                         [callback = std::move(callback)]() { callback(); });
    });
}

} // namespace cache
