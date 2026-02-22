// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"
#include "CacheApiWrappers.h"

#include <memory>
#include <utility>
#include <vector>

#include <QObject>

namespace cache {

void
onReadReceiptsChanged(QObject *receiver, std::function<void()> callback)
{
    QObject::connect(cacheInstance().get(),
                     &Cache::newReadReceipts,
                     receiver,
                     [callback = std::move(callback)](
                       const QString &, const std::vector<QString> &) { callback(); });
}

void
onReadReceiptsChanged(QObject *receiver,
                      std::function<void(const QString &, const std::vector<QString> &)> callback)
{
    QObject::connect(cacheInstance().get(),
                     &Cache::newReadReceipts,
                     receiver,
                     [callback = std::move(callback)](const QString &room_id,
                                                      const std::vector<QString> &event_ids) {
                         callback(room_id, event_ids);
                     });
}

void
onRoomReadStatusChanged(QObject *receiver, std::function<void(const std::map<QString, bool> &)> callback)
{
    QObject::connect(cacheInstance().get(),
                     &Cache::roomReadStatus,
                     receiver,
                     [callback = std::move(callback)](const std::map<QString, bool> &status) {
                         callback(status);
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
    QObject::connect(cacheInstance().get(),
                     &Cache::databaseReady,
                     receiver,
                     [callback = std::move(callback)]() { callback(); });
}

void
onSecretChanged(QObject *receiver, std::function<void(const std::string &)> callback)
{
    QObject::connect(cacheInstance().get(),
                     &Cache::secretChanged,
                     receiver,
                     [callback = std::move(callback)](const std::string &name) { callback(name); });
}

void
onVerificationStatusChanged(QObject *receiver, std::function<void(const std::string &)> callback)
{
    QObject::connect(cacheInstance().get(),
      &Cache::verificationStatusChanged,
      receiver,
      [callback = std::move(callback)](const std::string &user_id) { callback(user_id); });
}

void
onSelfVerificationStatusChanged(QObject *receiver, std::function<void()> callback)
{
    QObject::connect(cacheInstance().get(),
                     &Cache::selfVerificationStatusChanged,
                     receiver,
                     [callback = std::move(callback)]() { callback(); });
}

} // namespace cache
