// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <QTimer>

#include <mtx/errors.hpp>
#include <mtx/secret_storage.hpp>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace olm {

void
download_full_keybackup()
{
    if (!cache::isAvailable()) {
        if (const auto logger = nhlog::crypto()) {
            logger->debug("Skipping online key backup download: cache is not initialized yet.");
        }
        return;
    }

    if (!UserSettings::instance()->encryptionBackupOnlineEnabled()) {
        // Online key backup disabled
        nhlog::crypto()->debug("Not downloading full online key backup, because it is disabled.");
        return;
    }

    auto backupVersion = cache::backupVersion();
    if (!backupVersion) {
        // no trusted OKB
        nhlog::crypto()->debug(
          "Not downloading full online key backup, because we don't have a version for it.");
        return;
    }

    using namespace mtx::crypto;

    auto decryptedSecret = cache::secret(mtx::secret_storage::secrets::megolm_backup_v1);
    if (!decryptedSecret) {
        // no backup key available
        nhlog::crypto()->debug(
          "Not downloading full online key backup, because we don't have a key for it.");
        return;
    }
    auto sessionDecryptionKey = to_binary_buf(base642bin(*decryptedSecret));

    nhlog::crypto()->debug("Downloading full online key backup.");

    http::client()->room_keys(
      backupVersion->version,
      [sessionDecryptionKey](const mtx::responses::backup::KeysBackup &bk,
                             mtx::http::RequestErr err) {
          if (err) {
              if (err->status_code != 404)
                  nhlog::crypto()->error("Failed to dowload backup: {} - {}",
                                         mtx::errors::to_string(err->matrix_error.errcode),
                                         err->matrix_error.error);
              return;
          }
          nhlog::crypto()->debug("Storing full online key backup.");

          mtx::crypto::ExportedSessionKeys allKeys;
          for (const auto &[room, roomKey] : bk.rooms) {
              for (const auto &[session_id, encSession] : roomKey.sessions) {
                  try {
                      auto session = decrypt_session(encSession.session_data, sessionDecryptionKey);

                      if (session.algorithm != mtx::crypto::MEGOLM_ALGO)
                          // don't know this algorithm
                          return;

                      ExportedSession sess{};
                      sess.session_id = session_id;
                      sess.room_id    = room;
                      sess.algorithm  = mtx::crypto::MEGOLM_ALGO;
                      sess.forwarding_curve25519_key_chain =
                        std::move(session.forwarding_curve25519_key_chain);
                      sess.sender_claimed_keys = std::move(session.sender_claimed_keys);
                      sess.sender_key          = std::move(session.sender_key);
                      sess.session_key         = std::move(session.session_key);
                      allKeys.sessions.push_back(std::move(sess));
                  } catch (const olm_exception &e) {
                      nhlog::crypto()->critical("failed to decrypt inbound megolm session: {}",
                                                e.what());
                  }
              }
          }

          // call on UI thread
          QTimer::singleShot(0, ChatPage::instance(), [keys = std::move(allKeys)] {
              try {
                  cache::importSessionKeys(keys);
                  nhlog::crypto()->debug("Storing full online key backup completed.");
              } catch (const std::exception &e) {
                  nhlog::crypto()->critical("failed to save inbound megolm session: {}", e.what());
              }
          });
      });
}

void
lookup_keybackup(const std::string &room, const std::string &session_id)
{
    if (!UserSettings::instance()->encryptionBackupOnlineEnabled()) {
        // Online key backup disabled
        return;
    }

    auto backupVersion = cache::backupVersion();
    if (!backupVersion) {
        // no trusted OKB
        return;
    }

    using namespace mtx::crypto;

    auto decryptedSecret = cache::secret(mtx::secret_storage::secrets::megolm_backup_v1);
    if (!decryptedSecret) {
        // no backup key available
        return;
    }
    auto sessionDecryptionKey = to_binary_buf(base642bin(*decryptedSecret));

    http::client()->room_keys(
      backupVersion->version,
      room,
      session_id,
      [room, session_id, sessionDecryptionKey](const mtx::responses::backup::SessionBackup &bk,
                                               mtx::http::RequestErr err) {
          if (err) {
              if (err->status_code != 404)
                  nhlog::crypto()->error("Failed to dowload key {}:{}: {} - {}",
                                         room,
                                         session_id,
                                         mtx::errors::to_string(err->matrix_error.errcode),
                                         err->matrix_error.error);
              return;
          }
          try {
              auto session = decrypt_session(bk.session_data, sessionDecryptionKey);

              if (session.algorithm != mtx::crypto::MEGOLM_ALGO)
                  // don't know this algorithm
                  return;

              MegolmSessionIndex index;
              index.room_id    = room;
              index.session_id = session_id;

              GroupSessionData data{};
              data.forwarding_curve25519_key_chain = session.forwarding_curve25519_key_chain;
              data.sender_claimed_ed25519_key      = session.sender_claimed_keys["ed25519"];
              data.sender_key                      = session.sender_key;

              // online key backup can't be trusted, because anyone can upload to it.
              data.trusted = false;

              auto megolm_session =
                olm::client()->import_inbound_group_session(session.session_key);

              if (!cache::inboundMegolmSessionExists(index) ||
                  olm_inbound_group_session_first_known_index(megolm_session.get()) <
                    olm_inbound_group_session_first_known_index(
                      cache::getInboundMegolmSession(index).get())) {
                  cache::saveInboundMegolmSession(index, std::move(megolm_session), data);

                  nhlog::crypto()->info("imported inbound megolm session "
                                        "from key backup ({}, {})",
                                        room,
                                        session_id);

                  // call on UI thread
                  QTimer::singleShot(0, ChatPage::instance(), [index] {
                      ChatPage::instance()->receivedSessionKey(index.room_id, index.session_id);
                  });
              }
          } catch (const mtx::crypto::olm_exception &e) {
              nhlog::crypto()->critical("failed to import inbound megolm session: {}", e.what());
              return;
          } catch (const std::exception &e) {
              nhlog::crypto()->critical("failed to save inbound megolm session: {}", e.what());
              return;
          }
      });
}

} // namespace olm
