// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/SyncState.h"
#include "db/storage/Core.h"

namespace db::storage {

using db::getCacheFormatVersion;
using db::getCurrentOnlineBackupVersion;
using db::getNextBatchToken;
using db::getOlmAccount;
using db::getSyncStateJsonValue;
using db::getSyncStateSecretValue;
using db::getSyncStateValue;
using db::putCacheFormatVersion;
using db::putCurrentOnlineBackupVersion;
using db::putNextBatchToken;
using db::putOlmAccount;
using db::putSyncStateJsonValue;
using db::putSyncStateSecretValue;
using db::putSyncStateValue;
using db::removeCurrentOnlineBackupVersion;
using db::removeSyncStateSecretValue;
using db::removeSyncStateValue;

} // namespace db::storage
