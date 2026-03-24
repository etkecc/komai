// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cache/api/CacheApiTypes.h"

namespace cache {
void
onReadReceiptsChanged(QObject *receiver, std::function<void()> callback);
void
onReadReceiptsChanged(QObject *receiver,
                      std::function<void(const QString &, const std::vector<QString> &)> callback);
void
onRoomReadStatusChanged(QObject *receiver,
                        std::function<void(const std::map<QString, bool> &)> callback);
void
drainPendingCacheCallbacks();
void
disconnectFromCache(QObject *receiver);
void
onDatabaseReady(QObject *receiver, std::function<void()> callback);
void
onSecretChanged(QObject *receiver, std::function<void(const std::string &)> callback);
void
onVerificationStatusChanged(QObject *receiver, std::function<void(const std::string &)> callback);
void
onSelfVerificationStatusChanged(QObject *receiver, std::function<void()> callback);
} // namespace cache
