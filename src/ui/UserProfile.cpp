// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <mtx/errors.hpp>

#include "UserProfile.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "ui/UIA.h"
#include "utils/Utils.h"

namespace {
QString
requestErrorDetails(const mtx::http::ClientError &err)
{
    if (!err.matrix_error.error.empty())
        return QString::fromStdString(err.matrix_error.error);

    if (!err.parse_error.empty())
        return QString::fromStdString(err.parse_error);

    if (err.error_code != 0)
        return QString::fromLatin1(err.error_code_string());

    if (err.status_code != 0)
        return QStringLiteral("HTTP %1").arg(static_cast<int>(err.status_code));

    return {};
}
}

UserProfile::UserProfile(const QString &roomid,
                         const QString &userid,
                         TimelineViewManager *manager_,
                         TimelineModel *parent)
  : QObject(parent)
  , roomid_(roomid)
  , userid_(userid)
  , globalAvatarUrl{QLatin1String("")}
  , manager(manager_)
  , model(parent)
{
    connect(this,
            &UserProfile::globalUsernameRetrieved,
            this,
            &UserProfile::setGlobalUsername,
            Qt::QueuedConnection);
    connect(this,
            &UserProfile::verificationStatiChanged,
            this,
            &UserProfile::updateVerificationStatus,
            Qt::QueuedConnection);

    getGlobalProfileData();

    if (!cache::isDatabaseReady() || !ChatPage::instance()->timelineManager())
        return;

    cache::onVerificationStatusChanged(this, [this](const std::string &user_id) {
        if (user_id != this->userid_.toStdString())
            return;

        emit verificationStatiChanged();
    });
    fetchDeviceList(this->userid_);

    if (userid != utils::localUser())
        sharedRooms_ = new RoomInfoModel(cache::getCommonRooms(userid.toStdString()), this);
    else
        sharedRooms_ = new RoomInfoModel({}, this);

    connect(ChatPage::instance(), &ChatPage::syncUI, this, [this](const mtx::responses::Sync &res) {
        if (auto ignoreEv = std::ranges::find_if(
              res.account_data.events,
              [](const mtx::events::collections::RoomAccountDataEvents &e) {
                  return std::holds_alternative<
                    mtx::events::AccountDataEvent<mtx::events::account_data::IgnoredUsers>>(e);
              });
            ignoreEv != res.account_data.events.end()) {
            // doesn't matter much if it was actually us
            emit ignoredChanged();
        }
    });
}

DeviceInfoModel *
UserProfile::deviceList()
{
    return &this->deviceList_;
}

RoomInfoModel *
UserProfile::sharedRooms()
{
    return this->sharedRooms_;
}

QString
UserProfile::userid()
{
    return this->userid_;
}

QString
UserProfile::displayName()
{
    return isGlobalUserProfile() ? globalUsername : cache::displayName(roomid_, userid_);
}

QString
UserProfile::avatarUrl()
{
    return isGlobalUserProfile() ? globalAvatarUrl : cache::avatarUrl(roomid_, userid_);
}

bool
UserProfile::isGlobalUserProfile() const
{
    return roomid_ == QLatin1String("");
}

crypto::Trust
UserProfile::getUserStatus()
{
    return isUserVerified;
}

bool
UserProfile::userVerificationEnabled() const
{
    return hasMasterKey;
}
bool
UserProfile::isSelf() const
{
    return this->userid_ == utils::localUser();
}

void
UserProfile::signOutDevice(const QString &deviceID)
{
    nhlog::ui()->info(
      "Attempting to sign out device '{}' for user '{}' (is_self={}, local_device='{}')",
      deviceID.toStdString(),
      userid_.toStdString(),
      isSelf(),
      http::client()->device_id());
    http::client()->delete_device(
      deviceID.toStdString(),
      UIA::instance()->genericHandler(tr("Sign out device %1").arg(deviceID)),
      [this, deviceID](mtx::http::RequestErr e) {
          if (e) {
              nhlog::ui()->critical("Failed to sign out device '{}' for user '{}' "
                                    "(local_device='{}', status_code={}, errcode='{}', "
                                    "matrix_error='{}', parse_error='{}', error_code={}, "
                                    "error_code_string='{}')",
                                    deviceID.toStdString(),
                                    userid_.toStdString(),
                                    http::client()->device_id(),
                                    static_cast<int>(e->status_code),
                                    mtx::errors::to_string(e->matrix_error.errcode),
                                    e->matrix_error.error,
                                    e->parse_error,
                                    e->error_code,
                                    e->error_code_string());

              const auto details = requestErrorDetails(*e);
              MainWindow::instance()->showNotification(
                details.isEmpty()
                  ? tr("Failed to sign out device \"%1\".").arg(deviceID)
                  : tr("Failed to sign out device \"%1\": %2").arg(deviceID, details));
              return;
          }
          nhlog::ui()->info("Device {} successfully signed out!", deviceID.toStdString());
          // This is us. Let's update the interface accordingly
          if (isSelf() && deviceID.toStdString() == ::http::client()->device_id()) {
              ChatPage::instance()->dropToLoginPageCb(tr("You signed out this device."));
          }
          refreshDevices();
      });
}

void
UserProfile::refreshDevices()
{
    cache::markUserKeysOutOfDate({this->userid_.toStdString()});
    fetchDeviceList(this->userid_);
}

bool
UserProfile::ignored() const
{
    auto old = TimelineViewManager::instance()->getIgnoredUsers();
    return old.contains(userid_);
}

void
UserProfile::setIgnored(bool ignore)
{
    auto old = TimelineViewManager::instance()->getIgnoredUsers();
    if (ignore) {
        if (old.contains(userid_)) {
            emit ignoredChanged();
            return;
        }
        old.append(userid_);
    } else {
        if (!old.contains(userid_)) {
            emit ignoredChanged();
            return;
        }
        old.removeAll(userid_);
    }

    std::vector<mtx::events::account_data::IgnoredUser> content;
    for (const QString &item : std::as_const(old)) {
        content.push_back({item.toStdString()});
    }

    mtx::events::account_data::IgnoredUsers payload{.users{content}};

    auto userid = userid_;

    http::client()->put_account_data(payload, [userid](mtx::http::RequestErr e) {
        if (e) {
            MainWindow::instance()->showNotification(
              tr("Failed to ignore \"%1\": %2")
                .arg(userid, QString::fromStdString(e->matrix_error.error)));
        }
    });

    if (ignore) {
        const QHash<QString, RoomInfo> invites = cache::invites();
        FilteredRoomlistModel *room_model      = FilteredRoomlistModel::instance();

        for (auto room = invites.keyBegin(), end = invites.keyEnd(); room != end; room++) {
            if (room_model->getRoomPreviewById(*room).inviterUserId() == userid) {
                room_model->declineInvite(*room);
            }
        }
    }
}

void
UserProfile::fetchDeviceList(const QString &userID)
{
    if (!cache::isDatabaseReady())
        return;

    cache::queryKeys(userID.toStdString(),
                     [other_user_id = userID.toStdString(), this](const UserKeyCache &,
                                                                  mtx::http::RequestErr err) {
                         if (err) {
                             nhlog::net()->warn("failed to query device keys: {}", *err);
                         }

                         // Ensure local key cache is up to date
                         cache::queryKeys(
                           utils::localUser().toStdString(),
                           [this](const UserKeyCache &, mtx::http::RequestErr err) {
                               using namespace mtx;
                               std::string local_user_id = utils::localUser().toStdString();

                               if (err) {
                                   nhlog::net()->warn("failed to query device keys: {}", *err);
                               }

                               emit verificationStatiChanged();
                           });
                     });
}

void
UserProfile::updateVerificationStatus()
{
    if (!cache::isDatabaseReady())
        return;

    auto user_keys = cache::userKeys(userid_.toStdString());
    if (!user_keys) {
        this->hasMasterKey   = false;
        this->isUserVerified = crypto::Trust::Unverified;
        this->deviceList_.reset({});
        emit userStatusChanged();
        return;
    }

    this->hasMasterKey = !user_keys->master_keys.keys.empty();

    std::vector<DeviceInfo> deviceInfo;
    auto devices = user_keys->device_keys;
    auto verificationStatus =
      cache::verificationStatus(userid_.toStdString()).value_or(VerificationStatus{});

    this->isUserVerified = verificationStatus.user_verified;
    emit userStatusChanged();

    deviceInfo.reserve(devices.size());
    for (const auto &d : devices) {
        auto device = d.second;
        verification::Status verified =
          std::find(verificationStatus.verified_devices.begin(),
                    verificationStatus.verified_devices.end(),
                    device.device_id) == verificationStatus.verified_devices.end()
            ? verification::UNVERIFIED
            : verification::VERIFIED;

        if (isSelf() && device.device_id == ::http::client()->device_id())
            verified = verification::Status::SELF;

        deviceInfo.emplace_back(QString::fromStdString(d.first),
                                QString::fromStdString(device.unsigned_info.device_display_name),
                                verified);
    }

    // For self, also query devices without keys
    if (isSelf()) {
        http::client()->query_devices(
          [this, deviceInfo](const mtx::responses::QueryDevices &allDevs,
                             mtx::http::RequestErr err) mutable {
              if (err) {
                  nhlog::net()->warn("failed to query device keys: {}", *err);
                  this->deviceList_.queueReset(std::move(deviceInfo));
                  emit devicesChanged();
                  return;
              }
              for (const auto &d : allDevs.devices) {
                  // First, check if we already have an entry for this device
                  bool found = false;
                  for (auto &e : deviceInfo) {
                      if (e.device_id.toStdString() == d.device_id) {
                          found = true;
                          // Gottem! Let's fill in the blanks
                          e.lastIp = QString::fromStdString(d.last_seen_ip);
                          e.lastTs = static_cast<qlonglong>(d.last_seen_ts);
                          break;
                      }
                  }
                  // No entry? Let's add one.
                  if (!found) {
                      deviceInfo.emplace_back(QString::fromStdString(d.device_id),
                                              QString::fromStdString(d.display_name),
                                              verification::NOT_APPLICABLE,
                                              QString::fromStdString(d.last_seen_ip),
                                              d.last_seen_ts);
                  }
              }

              this->deviceList_.queueReset(std::move(deviceInfo));
              emit devicesChanged();
          });
        return;
    }

    this->deviceList_.queueReset(std::move(deviceInfo));
    emit devicesChanged();
}

void
UserProfile::setGlobalUsername(const QString &globalUser)
{
    globalUsername = globalUser;
    if (isGlobalUserProfile())
        emit displayNameChanged();
    emit globalDisplayNameChanged();
}

void
UserProfile::updateAvatarUrl()
{
    isLoading_ = false;
    emit loadingChanged();

    emit avatarUrlChanged();
}

bool
UserProfile::isLoading() const
{
    return isLoading_;
}

void
UserProfile::getGlobalProfileData()
{
    auto profProx = std::make_shared<UserProfileFetchProxy>();
    connect(profProx.get(),
            &UserProfileFetchProxy::profileFetched,
            this,
            [this](const mtx::responses::Profile &res) {
                emit globalUsernameRetrieved(QString::fromStdString(res.display_name));
                globalAvatarUrl = QString::fromStdString(res.avatar_url);
                if (isGlobalUserProfile())
                    emit avatarUrlChanged();
                emit globalAvatarUrlChanged();
            });

    connect(profProx.get(),
            &UserProfileFetchProxy::failedToFetchProfile,
            this,
            &UserProfile::failedToFetchProfile);

    http::client()->get_profile(userid_.toStdString(),
                                [prox = std::move(profProx), user = userid_.toStdString()](
                                  const mtx::responses::Profile &res, mtx::http::RequestErr err) {
                                    if (err) {
                                        nhlog::net()->warn("failed to retrieve profile info for {}",
                                                           user);
                                        emit prox->failedToFetchProfile();
                                        return;
                                    }

                                    emit prox->profileFetched(res);
                                });
}

#include "moc_UserProfile.cpp"
