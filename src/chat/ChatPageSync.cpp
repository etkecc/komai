// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include <mtx/responses.hpp>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "encryption/Olm.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/Permissions.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {
constexpr size_t MAX_ONETIME_KEYS = 50;
using InvitePermissionsContent    = mtx::events::account_data::nheko_extensions::InvitePermissions;
constexpr std::string_view KOMAI_INVITE_PERMISSIONS_TYPE = "cc.etke.komai.invite_permissions";

std::optional<InvitePermissionsContent>
parseInvitePermissionsFromRawAccountData(const std::string &eventJson)
{
    try {
        const auto parsedEvent = nlohmann::json::parse(eventJson);
        if (!parsedEvent.is_object() || !parsedEvent.contains("content"))
            return std::nullopt;

        return parsedEvent.at("content").get<InvitePermissionsContent>();
    } catch (const std::exception &) {
        return std::nullopt;
    }
}
} // namespace

void
ChatPage::tryInitialSync()
{
    nhlog::crypto()->info("ed25519   : {}", olm::client()->identity_keys().ed25519);
    nhlog::crypto()->info("curve25519: {}", olm::client()->identity_keys().curve25519);

    // Upload one time keys for the device.
    nhlog::crypto()->info("generating one time keys");
    olm::client()->generate_one_time_keys(MAX_ONETIME_KEYS, true);

    http::client()->upload_keys(
      olm::client()->create_upload_keys_request(),
      [this](const mtx::responses::UploadKeys &res, mtx::http::RequestErr err) {
          if (shuttingDown_)
              return;

          if (err) {
              const int status_code = static_cast<int>(err->status_code);

              if (status_code == 404) {
                  nhlog::net()->warn("skipping key uploading. server doesn't provide /keys/upload");
                  return startInitialSync();
              }

              nhlog::crypto()->critical("failed to upload one time keys: {}", err);

              QString errorMsg(tr("Failed to setup encryption keys. Server response: %1 %2. Please "
                                  "try again later.")
                                 .arg(QString::fromStdString(err->matrix_error.error))
                                 .arg(status_code));

              emit dropToLoginPageCb(errorMsg);
              return;
          }

          olm::client()->forget_old_fallback_key();
          olm::mark_keys_as_published();

          for (const auto &entry : res.one_time_key_counts)
              nhlog::net()->info("uploaded {} {} one-time keys", entry.second, entry.first);

          cache::markUserKeysOutOfDate({http::client()->user_id().to_string()});

          startInitialSync();
      });
}

void
ChatPage::startInitialSync()
{
    nhlog::net()->info("trying initial sync");

    mtx::http::SyncOpts opts;
    opts.timeout      = 0;
    opts.set_presence = currentPresence();

    http::client()->sync(opts, [this](const mtx::responses::Sync &res, mtx::http::RequestErr err) {
        if (shuttingDown_)
            return;

        // TODO: Initial Sync should include mentions as well...

        if (err) {
            const auto error      = QString::fromStdString(err->matrix_error.error);
            const auto msg        = tr("Please try to login again: %1").arg(error);
            const auto err_code   = mtx::errors::to_string(err->matrix_error.errcode);
            const int status_code = static_cast<int>(err->status_code);

            nhlog::net()->error("initial sync error: {}", err);

            // non http related errors
            if (status_code <= 0 || status_code >= 600) {
                startInitialSync();
                return;
            }

            switch (status_code) {
            case 502:
            case 504:
            case 524: {
                startInitialSync();
                return;
            }
            default: {
                emit dropToLoginPageCb(msg);
                return;
            }
            }
        }

        QTimer::singleShot(0, this, [this, res] {
            nhlog::net()->info("initial sync completed");
            try {
                cache::saveState(res);

                olm::handle_to_device_messages(res.to_device.events);
                const auto localUserId = utils::localUser().toStdString();
                view_manager_->sync(komai::buildSyncUpdate(res, localUserId));

                cache::calculateRoomReadStatus();
            } catch (const std::exception &e) {
                nhlog::db()->error("failed to save state after initial sync: {}", e.what());
                startInitialSync();
                return;
            }

            emit trySyncCb();
            emit contentLoaded();
        });
    });
}

void
ChatPage::handleSyncResponse(const mtx::responses::Sync &res, const std::string &prev_batch_token)
{
    try {
        if (prev_batch_token != cache::nextBatchToken()) {
            nhlog::net()->warn("Duplicate sync, dropping");
            return;
        }
    } catch (const std::exception &) {
        nhlog::db()->warn("Logged out in the mean time, dropping sync");
        return;
    }

    nhlog::net()->debug("sync completed: {}", res.next_batch);

    // Ensure that we have enough one-time keys available.
    std::map<std::string_view, std::uint16_t> counts{res.device_one_time_keys_count.begin(),
                                                     res.device_one_time_keys_count.end()};
    ensureOneTimeKeyCount(counts, res.device_unused_fallback_key_types);

    std::optional<mtx::events::account_data::IgnoredUsers> oldIgnoredUsers;
    if (auto ignoreEv = std::ranges::find_if(
          res.account_data.events,
          [](const mtx::events::collections::RoomAccountDataEvents &e) {
              return std::holds_alternative<
                mtx::events::AccountDataEvent<mtx::events::account_data::IgnoredUsers>>(e);
          });
        ignoreEv != res.account_data.events.end()) {
        if (auto oldEv = cache::getAccountData(mtx::events::EventType::IgnoredUsers))
            oldIgnoredUsers =
              std::get<mtx::events::AccountDataEvent<mtx::events::account_data::IgnoredUsers>>(
                *oldEv)
                .content;
        else
            oldIgnoredUsers = mtx::events::account_data::IgnoredUsers{};
    }

    // TODO: fine grained error handling
    try {
        cache::saveState(res);
        olm::handle_to_device_messages(res.to_device.events);
        const auto localUserId = utils::localUser().toStdString();
        for (const auto &presence : res.presence) {
            if (presence.sender == localUserId) {
                statusMessageShadow_ = QString::fromStdString(presence.content.status_msg);
                break;
            }
        }

        // reject forbidden invites
        if (!res.rooms.invite.empty()) {
            if (auto raw =
                  cache::getAccountDataByType(std::string(KOMAI_INVITE_PERMISSIONS_TYPE))) {
                if (auto invitePerms = parseInvitePermissionsFromRawAccountData(*raw)) {
                    for (const auto &[roomid, invite] : res.rooms.invite) {
                        std::string_view inviter = "";
                        for (const auto &memberEv : invite.invite_state) {
                            if (auto member = std::get_if<
                                  mtx::events::StrippedEvent<mtx::events::state::Member>>(
                                  &memberEv)) {
                                if (member->content.membership ==
                                      mtx::events::state::Membership::Invite &&
                                    member->state_key == localUserId) {
                                    inviter = member->sender;
                                    break;
                                }
                            }
                        }

                        if (!invitePerms->invite_allowed(roomid, inviter)) {
                            leaveRoom(QString::fromStdString(roomid), "");
                        }
                    }
                }
            }
        }

        view_manager_->sync(komai::buildSyncUpdate(res, localUserId));
        processSyncUi(komai::buildNotificationSyncUpdate(res));

        // if the ignored users changed, clear timeline of all affected rooms.
        if (oldIgnoredUsers) {
            if (auto newEv = cache::getAccountData(mtx::events::EventType::IgnoredUsers)) {
                std::vector<mtx::events::account_data::IgnoredUser> changedUsers{};
                std::ranges::set_symmetric_difference(
                  oldIgnoredUsers->users,
                  std::get<mtx::events::AccountDataEvent<mtx::events::account_data::IgnoredUsers>>(
                    *newEv)
                    .content.users,
                  std::back_inserter(changedUsers),
                  {},
                  &mtx::events::account_data::IgnoredUser::id,
                  &mtx::events::account_data::IgnoredUser::id);

                std::unordered_set<std::string> roomsToReload;
                for (const auto &user : changedUsers) {
                    auto commonRooms = cache::getCommonRooms(user.id);
                    for (const auto &room : commonRooms)
                        roomsToReload.insert(room.first);
                }

                for (const auto &room : roomsToReload) {
                    if (auto model = view_manager_->rooms()->getMaterializedRoomById(
                          QString::fromStdString(room))) {
                        model->clearTimeline();
                    } else {
                        cache::clearTimeline(room);
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        if (cache::isMapFullError(e)) {
            nhlog::db()->error("storage is full: {}", e.what());
            cache::deleteOldData();
        } else {
            nhlog::db()->error("saving sync response: {}", e.what());
        }
    }

    if (shouldThrottleSync())
        QTimer::singleShot(1000, this, &ChatPage::trySyncCb);
    else
        emit trySyncCb();
}

void
ChatPage::trySync()
{
    mtx::http::SyncOpts opts;
    opts.set_presence = currentPresence();

    if (!connectivityTimer_.isActive())
        connectivityTimer_.start();

    try {
        opts.since = cache::nextBatchToken();
    } catch (const std::exception &e) {
        nhlog::db()->error("failed to retrieve next batch token: {}", e.what());
        return;
    }

    http::client()->sync(
      opts, [this, since = opts.since](const mtx::responses::Sync &res, mtx::http::RequestErr err) {
          if (shuttingDown_)
              return;

          if (err) {
              const auto error = QString::fromStdString(err->matrix_error.error);
              const auto msg   = tr("Please try to login again: %1").arg(error);

              if ((http::is_logged_in() &&
                   (err->matrix_error.errcode == mtx::errors::ErrorCode::M_UNKNOWN_TOKEN ||
                    err->matrix_error.errcode == mtx::errors::ErrorCode::M_MISSING_TOKEN)) ||
                  !http::is_logged_in()) {
                  emit dropToLoginPageCb(msg);
                  return;
              }

              nhlog::net()->error("sync error: {}", *err);
              if (isConnected_)
                  emit connectionLost();
              emit tryDelayedSyncCb();
              return;
          }

          if (!isConnected_)
              emit connectionRestored();
          emit newSyncResponse(res, since);
      });
}

QString
ChatPage::status() const
{
    if (statusMessageShadow_.has_value())
        return *statusMessageShadow_;

    return QString::fromStdString(cache::presence(utils::localUser().toStdString()).status_msg);
}

void
ChatPage::setStatus(const QString &status)
{
    statusMessageShadow_ = status;

    http::client()->put_presence_status(
      currentPresence(), status.toStdString(), [this](mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to set presence status_msg: {}", err->matrix_error.error);
              statusMessageShadow_.reset();
          }
      });
}

bool
ChatPage::shouldBeUnavailable() const
{
    return lastWindowActive.isValid() &&
           lastWindowActive.addSecs(60 * 5) < QDateTime::currentDateTime();
}

bool
ChatPage::shouldThrottleSync() const
{
    return lastWindowActive.isValid() &&
           lastWindowActive.addSecs(6 * 5) < QDateTime::currentDateTime();
}

mtx::presence::PresenceState
ChatPage::currentPresence() const
{
    switch (userSettings_->networkPresenceStatusPolicy()) {
    case UserSettings::Presence::Online:
        return mtx::presence::online;
    case UserSettings::Presence::Unavailable:
        return mtx::presence::unavailable;
    case UserSettings::Presence::Offline:
        return mtx::presence::offline;
    case UserSettings::Presence::AutomaticPresence:
        if (shouldBeUnavailable())
            return mtx::presence::unavailable;
        else
            return mtx::presence::online;

    default:
        return mtx::presence::online;
    }
}

void
ChatPage::verifyOneTimeKeyCountAfterStartup()
{
    nhlog::crypto()->info("verifyOneTimeKeyCountAfterStartup: device_id={}",
                          http::client()->device_id());

    http::client()->upload_keys(
      olm::client()->create_upload_keys_request(),
      [this](const mtx::responses::UploadKeys &res, mtx::http::RequestErr err) {
          if (err) {
              nhlog::crypto()->warn("failed to update one-time keys: {} "
                                    "(device_id={})",
                                    err,
                                    http::client()->device_id());

              if (err->status_code < 400 || err->status_code >= 500)
                  return;
          }

          std::map<std::string_view, uint16_t> key_counts;
          std::uint64_t count = 0;
          if (auto c = res.one_time_key_counts.find(mtx::crypto::SIGNED_CURVE25519);
              c == res.one_time_key_counts.end()) {
              key_counts[mtx::crypto::SIGNED_CURVE25519] = 0;
          } else {
              key_counts[mtx::crypto::SIGNED_CURVE25519] =
                c->second > std::numeric_limits<std::uint16_t>::max()
                  ? std::numeric_limits<std::uint16_t>::max()
                  : static_cast<std::uint16_t>(c->second);
              count = c->second;
          }

          nhlog::crypto()->info(
            "Fetched server key count {} {}", count, mtx::crypto::SIGNED_CURVE25519);

          ensureOneTimeKeyCount(key_counts, std::nullopt);
      });
}

void
ChatPage::ensureOneTimeKeyCount(const std::map<std::string_view, uint16_t> &counts,
                                const std::optional<std::vector<std::string>> &unused_fallback_keys)
{
    if (auto count = counts.find(mtx::crypto::SIGNED_CURVE25519); count != counts.end()) {
        bool replace_fallback_key = false;
        if (unused_fallback_keys &&
            std::find(unused_fallback_keys->begin(),
                      unused_fallback_keys->end(),
                      mtx::crypto::SIGNED_CURVE25519) == unused_fallback_keys->end())
            replace_fallback_key = true;
        nhlog::crypto()->debug(
          "Updated server key count {} {}, fallback keys supported: {}, new fallback key: {}",
          count->second,
          mtx::crypto::SIGNED_CURVE25519,
          unused_fallback_keys.has_value(),
          replace_fallback_key);

        if (count->second < MAX_ONETIME_KEYS || replace_fallback_key) {
            const size_t nkeys =
              count->second < MAX_ONETIME_KEYS ? (MAX_ONETIME_KEYS - count->second) : 0;

            nhlog::crypto()->info("uploading {} {} keys", nkeys, mtx::crypto::SIGNED_CURVE25519);
            olm::client()->generate_one_time_keys(nkeys, replace_fallback_key);

            http::client()->upload_keys(
              olm::client()->create_upload_keys_request(),
              [replace_fallback_key, this](const mtx::responses::UploadKeys &,
                                           mtx::http::RequestErr err) {
                  if (err) {
                      nhlog::crypto()->warn("failed to update one-time keys: {} "
                                            "(device_id={})",
                                            err,
                                            http::client()->device_id());

                      if (err->status_code < 400 || err->status_code >= 500)
                          return;
                  }

                  // mark as published anyway, otherwise we may end up in a loop.
                  olm::mark_keys_as_published();

                  if (replace_fallback_key) {
                      emit startRemoveFallbackKeyTimer();
                  }
              });
        } else if (count->second > 2 * MAX_ONETIME_KEYS) {
            nhlog::crypto()->warn("too many one-time keys, deleting 1");
            mtx::requests::ClaimKeys req;
            req.one_time_keys[http::client()->user_id().to_string()][http::client()->device_id()] =
              std::string(mtx::crypto::SIGNED_CURVE25519);
            http::client()->claim_keys(
              req, [](const mtx::responses::ClaimKeys &, mtx::http::RequestErr err) {
                  if (err)
                      nhlog::crypto()->warn("failed to clear 1 one-time key: {}", err);
                  else
                      nhlog::crypto()->info("cleared 1 one-time key");
              });
        }
    }
}

void
ChatPage::removeOldFallbackKey()
{
    olm::client()->forget_old_fallback_key();
    olm::mark_keys_as_published();
}

void
ChatPage::getProfileInfo()
{
    const auto userid = utils::localUser().toStdString();

    http::client()->get_profile(
      userid, [this](const mtx::responses::Profile &res, mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to retrieve own profile info");
              return;
          }

          emit setUserDisplayName(QString::fromStdString(res.display_name));

          emit setUserAvatar(QString::fromStdString(res.avatar_url));
      });
}

bool
ChatPage::isRoomActive(const QString &room_id)
{
    return QGuiApplication::focusWindow() && QGuiApplication::focusWindow()->isActive() &&
           MainWindow::instance()->windowForRoom(room_id) == QGuiApplication::focusWindow();
}
