// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <string_view>

#include <nlohmann/json.hpp>

#include "Logging.h"
#include "db/Json.h"
#include "db/OlmSessionIndex.h"
#include "db/Serde.h"
#include "db/StorageApi.h"
#include "db/SyncState.h"
#include "encryption/Olm.h"

void
MatrixStore::saveOlmSessions(
  std::vector<std::pair<std::string, mtx::crypto::OlmSessionPtr>> sessions,
  uint64_t timestamp)
{
    using namespace mtx::crypto;

    auto txn = beginTxn();
    for (const auto &[curve25519, session] : sessions) {
        const auto pickled    = pickle<SessionObject>(session.get(), pickle_secret_);
        const auto session_id = mtx::crypto::session_id(session.get());

        StoredOlmSession stored_session;
        stored_session.pickled_session = pickled;
        stored_session.last_message_ts = timestamp;

        db::putOlmSessionValue(
          txn, db->olmSessions, curve25519, session_id, nlohmann::json(stored_session).dump());
    }

    txn.commit();
}

void
MatrixStore::saveOlmSession(const std::string &curve25519,
                            mtx::crypto::OlmSessionPtr session,
                            uint64_t timestamp)
{
    using namespace mtx::crypto;

    auto txn = beginTxn();

    const auto pickled    = pickle<SessionObject>(session.get(), pickle_secret_);
    const auto session_id = mtx::crypto::session_id(session.get());

    StoredOlmSession stored_session;
    stored_session.pickled_session = pickled;
    stored_session.last_message_ts = timestamp;

    db::putOlmSessionValue(
      txn, db->olmSessions, curve25519, session_id, nlohmann::json(stored_session).dump());

    txn.commit();
}

std::optional<mtx::crypto::OlmSessionPtr>
MatrixStore::getOlmSession(const std::string &curve25519, const std::string &session_id)
{
    using namespace mtx::crypto;

    try {
        auto txn = ro_txn(storage());

        if (auto data = db::getJsonValue<StoredOlmSession>(
              txn, db->olmSessions, db::catalog::olmSessionKey(curve25519, session_id))) {
            return unpickle<SessionObject>(data->pickled_session, pickle_secret_);
        }

    } catch (...) {
    }
    return std::nullopt;
}

std::optional<mtx::crypto::OlmSessionPtr>
MatrixStore::getLatestOlmSession(const std::string &curve25519)
{
    using namespace mtx::crypto;

    try {
        auto txn = ro_txn(storage());

        std::optional<StoredOlmSession> currentNewest;
        db::forEachOlmSessionForCurve(
          txn,
          db->olmSessions,
          curve25519,
          [&currentNewest, &txn, this, &curve25519](std::string_view sessionId,
                                                    std::string_view /*pickled_session*/) {
              auto data = db::getJsonValue<StoredOlmSession>(
                txn, db->olmSessions, db::catalog::olmSessionKey(curve25519, sessionId));
              if (!data)
                  return true;

              if (!currentNewest || currentNewest->last_message_ts < data->last_message_ts)
                  currentNewest = *data;
              return true;
          });

        return currentNewest ? std::optional(unpickle<SessionObject>(currentNewest->pickled_session,
                                                                     pickle_secret_))
                             : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string>
MatrixStore::getOlmSessions(const std::string &curve25519)
{
    using namespace mtx::crypto;

    try {
        auto txn = ro_txn(storage());
        return db::listOlmSessionIds(txn, db->olmSessions, curve25519);
    } catch (...) {
        return {};
    }
}

void
MatrixStore::saveOlmAccount(const std::string &data)
{
    auto txn = beginTxn();
    db::putSyncStateValue(txn, db->syncState, db::catalog::SyncStateKey::OlmAccount, data);
    txn.commit();
}

std::string
MatrixStore::restoreOlmAccount()
{
    auto txn = ro_txn(storage());
    return db::getSyncStateValue(txn, db->syncState, db::catalog::SyncStateKey::OlmAccount)
      .value_or("");
}

void
MatrixStore::saveBackupVersion(const OnlineBackupVersion &data)
{
    auto txn = beginTxn();
    db::putSyncStateJsonValue(
      txn, db->syncState, db::catalog::SyncStateKey::CurrentOnlineBackupVersion, data);
    txn.commit();
}

void
MatrixStore::deleteBackupVersion()
{
    auto txn = beginTxn();
    db::removeSyncStateValue(
      txn, db->syncState, db::catalog::SyncStateKey::CurrentOnlineBackupVersion);
    txn.commit();
}

std::optional<OnlineBackupVersion>
MatrixStore::backupVersion()
{
    try {
        auto txn   = ro_txn(storage());
        auto value = db::getSyncStateJsonValue<OnlineBackupVersion>(
          txn, db->syncState, db::catalog::SyncStateKey::CurrentOnlineBackupVersion);
        if (!value)
            return std::nullopt;

        return value;
    } catch (...) {
        return std::nullopt;
    }
}
