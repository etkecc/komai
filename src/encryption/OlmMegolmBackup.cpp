// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <QDateTime>
#include <QTimer>

#include <mtx/secret_storage.hpp>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
constexpr auto MEGOLM_ALGO = "m.megolm.v1.aes-sha2";
}

namespace olm {
static void
backup_session_key(const MegolmSessionIndex &idx,
                   const GroupSessionData &data,
                   mtx::crypto::InboundGroupSessionPtr &session);

mtx::events::msg::Encrypted
encrypt_group_message(const std::string &room_id, const std::string &device_id, nlohmann::json body)
{
    using namespace mtx::events;
    using namespace mtx::identifiers;

    auto own_user_id = http::client()->user_id().to_string();

    auto members = cache::getMembersWithKeys(
      room_id, UserSettings::instance()->encryptionKeySharingOnlyVerifiedUsers());

    std::map<std::string, std::vector<std::string>> sendSessionTo;
    mtx::crypto::OutboundGroupSessionPtr session = nullptr;
    GroupSessionData group_session_data;

    if (cache::outboundMegolmSessionExists(room_id)) {
        auto res                = cache::getOutboundMegolmSession(room_id);
        auto encryptionSettings = cache::roomEncryptionSettings(room_id);
        mtx::events::state::Encryption defaultSettings;

        // rotate if we crossed the limits for this key
        if (res.data.message_index <
              encryptionSettings.value_or(defaultSettings).rotation_period_msgs &&
            (QDateTime::currentMSecsSinceEpoch() - res.data.timestamp) <
              encryptionSettings.value_or(defaultSettings).rotation_period_ms) {
            auto member_it             = members.begin();
            auto session_member_it     = res.data.currently.keys.begin();
            auto session_member_it_end = res.data.currently.keys.end();

            while (member_it != members.end() || session_member_it != session_member_it_end) {
                if (member_it == members.end()) {
                    // a member left, purge session!
                    nhlog::crypto()->debug("Rotating megolm session because of left member");
                    break;
                }

                if (session_member_it == session_member_it_end) {
                    // share with all remaining members
                    while (member_it != members.end()) {
                        sendSessionTo[member_it->first] = {};

                        if (member_it->second)
                            for (const auto &dev : member_it->second->device_keys)
                                if (member_it->first != own_user_id || dev.first != device_id)
                                    sendSessionTo[member_it->first].push_back(dev.first);

                        ++member_it;
                    }

                    session = std::move(res.session);
                    break;
                }

                if (member_it->first > session_member_it->first) {
                    // a member left, purge session
                    nhlog::crypto()->debug("Rotating megolm session because of left member");
                    break;
                } else if (member_it->first < session_member_it->first) {
                    // new member, send them the session at this index
                    sendSessionTo[member_it->first] = {};

                    if (member_it->second) {
                        for (const auto &dev : member_it->second->device_keys)
                            if (member_it->first != own_user_id || dev.first != device_id)
                                sendSessionTo[member_it->first].push_back(dev.first);
                    }

                    ++member_it;
                } else {
                    // compare devices
                    bool device_removed = false;
                    for (const auto &dev : session_member_it->second.deviceids) {
                        if (!member_it->second ||
                            !member_it->second->device_keys.count(dev.first)) {
                            device_removed = true;
                            break;
                        }
                    }

                    if (device_removed) {
                        // device removed, rotate session!
                        nhlog::crypto()->debug("Rotating megolm session because of removed "
                                               "device of {}",
                                               member_it->first);
                        break;
                    }

                    // check for new devices to share with
                    if (member_it->second)
                        for (const auto &dev : member_it->second->device_keys)
                            if (!session_member_it->second.deviceids.count(dev.first) &&
                                (member_it->first != own_user_id || dev.first != device_id))
                                sendSessionTo[member_it->first].push_back(dev.first);

                    ++member_it;
                    ++session_member_it;
                    if (member_it == members.end() && session_member_it == session_member_it_end) {
                        // all devices match or are newly added
                        session = std::move(res.session);
                    }
                }
            }
        }

        group_session_data = std::move(res.data);
    }

    if (!session) {
        nhlog::ui()->debug("creating new outbound megolm session");

        // Create a new outbound megolm session.
        session                = olm::client()->init_outbound_group_session();
        const auto session_id  = mtx::crypto::session_id(session.get());
        const auto session_key = mtx::crypto::session_key(session.get());

        // Saving the new megolm session.
        GroupSessionData session_data{};
        session_data.message_index              = 0;
        session_data.trusted                    = true;
        session_data.timestamp                  = QDateTime::currentMSecsSinceEpoch();
        session_data.sender_claimed_ed25519_key = olm::client()->identity_keys().ed25519;
        session_data.sender_key                 = olm::client()->identity_keys().curve25519;

        sendSessionTo.clear();

        for (const auto &[user, devices] : members) {
            sendSessionTo[user]               = {};
            session_data.currently.keys[user] = {};
            if (devices) {
                for (const auto &[device_id_, key] : devices->device_keys) {
                    (void)key;
                    if (device_id != device_id_ || user != own_user_id) {
                        sendSessionTo[user].push_back(device_id_);
                        session_data.currently.keys[user].deviceids[device_id_] = 0;
                    }
                }
            }
        }

        {
            MegolmSessionIndex index;
            index.room_id       = room_id;
            index.session_id    = session_id;
            auto megolm_session = olm::client()->init_inbound_group_session(session_key);
            backup_session_key(index, session_data, megolm_session);
            cache::saveInboundMegolmSession(index, std::move(megolm_session), session_data);
        }

        cache::saveOutboundMegolmSession(room_id, session_data, session);
        group_session_data = std::move(session_data);
    }

    mtx::events::DeviceEvent<mtx::events::msg::RoomKey> megolm_payload{};
    megolm_payload.content.algorithm   = MEGOLM_ALGO;
    megolm_payload.content.room_id     = room_id;
    megolm_payload.content.session_id  = mtx::crypto::session_id(session.get());
    megolm_payload.content.session_key = mtx::crypto::session_key(session.get());
    megolm_payload.type                = mtx::events::EventType::RoomKey;

    if (!sendSessionTo.empty())
        olm::send_encrypted_to_device_messages(sendSessionTo, megolm_payload);

    auto data = encrypt_group_message_with_session(session, device_id, body);

    group_session_data.message_index = olm_outbound_group_session_message_index(session.get());
    nhlog::crypto()->debug("next message_index {}", group_session_data.message_index);

    // update current set of members for the session with the new members and that message_index
    for (const auto &[user, devices] : sendSessionTo) {
        if (!group_session_data.currently.keys.count(user))
            group_session_data.currently.keys[user] = {};

        for (const auto &device_id_ : devices) {
            if (!group_session_data.currently.keys[user].deviceids.count(device_id_))
                group_session_data.currently.keys[user].deviceids[device_id_] =
                  group_session_data.message_index;
        }
    }

    // We need to re-pickle the session after we send a message to save the new message_index.
    cache::updateOutboundMegolmSession(room_id, group_session_data, session);

    return data;
}

void
create_inbound_megolm_session(const mtx::events::DeviceEvent<mtx::events::msg::RoomKey> &roomKey,
                              const std::string &sender_key,
                              const std::string &sender_ed25519)
{
    MegolmSessionIndex index;
    index.room_id    = roomKey.content.room_id;
    index.session_id = roomKey.content.session_id;

    try {
        auto megolm_session =
          olm::client()->init_inbound_group_session(roomKey.content.session_key);

        GroupSessionData data{};
        data.forwarding_curve25519_key_chain = {sender_key};
        data.sender_claimed_ed25519_key      = sender_ed25519;
        data.sender_key                      = sender_key;

        data.trusted = olm_inbound_group_session_is_verified(megolm_session.get());

        backup_session_key(index, data, megolm_session);
        cache::saveInboundMegolmSession(index, std::move(megolm_session), data);
    } catch (const mtx::crypto::olm_exception &e) {
        nhlog::crypto()->critical("failed to create inbound megolm session: {}", e.what());
        return;
    } catch (const std::exception &e) {
        nhlog::crypto()->critical("failed to save inbound megolm session: {}", e.what());
        return;
    }

    nhlog::crypto()->info(
      "established inbound megolm session ({}, {})", roomKey.content.room_id, roomKey.sender);

    ChatPage::instance()->receivedSessionKey(index.room_id, index.session_id);
}

void
import_inbound_megolm_session(
  const mtx::events::DeviceEvent<mtx::events::msg::ForwardedRoomKey> &roomKey)
{
    MegolmSessionIndex index;
    index.room_id    = roomKey.content.room_id;
    index.session_id = roomKey.content.session_id;

    try {
        auto megolm_session =
          olm::client()->import_inbound_group_session(roomKey.content.session_key);

        GroupSessionData data{};
        data.forwarding_curve25519_key_chain = roomKey.content.forwarding_curve25519_key_chain;
        data.sender_claimed_ed25519_key      = roomKey.content.sender_claimed_ed25519_key;
        data.sender_key                      = roomKey.content.sender_key;
        // Keys from online key backup won't have a signature, so they will be untrusted. But the
        // original sender might send us a signed session.
        data.trusted = olm_inbound_group_session_is_verified(megolm_session.get());

        backup_session_key(index, data, megolm_session);
        cache::saveInboundMegolmSession(index, std::move(megolm_session), data);
    } catch (const mtx::crypto::olm_exception &e) {
        nhlog::crypto()->critical("failed to import inbound megolm session: {}", e.what());
        return;
    } catch (const std::exception &e) {
        nhlog::crypto()->critical("failed to save inbound megolm session: {}", e.what());
        return;
    }

    nhlog::crypto()->info(
      "established inbound megolm session ({}, {})", roomKey.content.room_id, roomKey.sender);

    ChatPage::instance()->receivedSessionKey(index.room_id, index.session_id);
}

static void
backup_session_key(const MegolmSessionIndex &idx,
                   const GroupSessionData &data,
                   mtx::crypto::InboundGroupSessionPtr &session)
{
    try {
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

        auto public_key = mtx::crypto::CURVE25519_public_key_from_private(sessionDecryptionKey);

        mtx::responses::backup::SessionData sessionData;
        sessionData.algorithm                       = mtx::crypto::MEGOLM_ALGO;
        sessionData.forwarding_curve25519_key_chain = data.forwarding_curve25519_key_chain;
        sessionData.sender_claimed_keys["ed25519"]  = data.sender_claimed_ed25519_key;
        sessionData.sender_key                      = data.sender_key;
        sessionData.session_key = mtx::crypto::export_session(session.get(), -1);

        auto encrypt_session = mtx::crypto::encrypt_session(sessionData, public_key);

        mtx::responses::backup::SessionBackup bk;
        bk.first_message_index = olm_inbound_group_session_first_known_index(session.get());
        bk.forwarded_count     = data.forwarding_curve25519_key_chain.size();
        bk.is_verified         = false;
        bk.session_data        = std::move(encrypt_session);

        http::client()->put_room_keys(
          backupVersion->version,
          idx.room_id,
          idx.session_id,
          bk,
          [idx](mtx::http::RequestErr err) {
              if (err) {
                  nhlog::net()->warn("failed to backup session key ({}:{}): {} ({})",
                                     idx.room_id,
                                     idx.session_id,
                                     err->matrix_error.error,
                                     static_cast<int>(err->status_code));
              } else {
                  nhlog::crypto()->debug(
                    "backed up session key ({}:{})", idx.room_id, idx.session_id);
              }
          });
    } catch (std::exception &e) {
        nhlog::net()->warn("failed to backup session key: {}", e.what());
    }
}

void
mark_keys_as_published()
{
    olm::client()->mark_keys_as_published();
    auto secret = cache::pickleSecret();
    if (!secret.empty())
        cache::saveOlmAccount(olm::client()->save(secret));
    else
        nhlog::crypto()->warn("skipping OLM account save: pickle secret unavailable");
}

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
