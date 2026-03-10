// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFileDialog>
#include <QMimeDatabase>
#include <QStandardPaths>

#include "UserProfile.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "encryption/VerificationManager.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "timeline/TimelineViewManager.h"
#include "utils/Utils.h"

void
UserProfile::banUser()
{
    ChatPage::instance()->banUser(roomid_, this->userid_, QLatin1String(""));
}

void
UserProfile::kickUser()
{
    ChatPage::instance()->kickUser(roomid_, this->userid_, QLatin1String(""));
}

void
UserProfile::startChat(bool encryption)
{
    ChatPage::instance()->startChat(this->userid_, encryption);
}

void
UserProfile::startChat()
{
    ChatPage::instance()->startChat(this->userid_, std::nullopt);
}

void
UserProfile::changeUsername(const QString &username)
{
    if (isGlobalUserProfile()) {
        // change global
        http::client()->set_displayname(username.toStdString(), [](mtx::http::RequestErr err) {
            if (err) {
                nhlog::net()->warn("could not change username: {}", *err);
                return;
            }
        });
    } else {
        // change room username
        mtx::events::state::Member member;
        member.display_name = username.toStdString();
        member.avatar_url   = cache::avatarUrl(roomid_, utils::localUser()).toStdString();
        member.membership   = mtx::events::state::Membership::Join;

        updateRoomMemberState(std::move(member));
    }
}

void
UserProfile::changeDeviceName(const QString &deviceID, const QString &deviceName)
{
    http::client()->set_device_name(
      deviceID.toStdString(), deviceName.toStdString(), [this](mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("could not change device name: {}", *err);
              return;
          }
          refreshDevices();
      });
}

void
UserProfile::verify(QString device)
{
    if (!device.isEmpty())
        manager->verificationManager()->verifyDevice(userid_, device);
    else {
        manager->verificationManager()->verifyUser(userid_);
    }
}

void
UserProfile::unverify(const QString &device)
{
    cache::markDeviceUnverified(userid_.toStdString(), device.toStdString());
}

void
UserProfile::changeAvatar()
{
    const QString picturesFolder =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString fileName = QFileDialog::getOpenFileName(
      nullptr, tr("Select an avatar"), picturesFolder, tr("All Files (*)"));

    if (fileName.isEmpty())
        return;

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(fileName, QMimeDatabase::MatchContent);

    const auto format = mime.name().split(QStringLiteral("/"))[0];

    QFile file{fileName, this};
    if (format != QLatin1String("image")) {
        emit displayError(tr("The selected file is not an image"));
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        emit displayError(tr("Error while reading file: %1").arg(file.errorString()));
        return;
    }

    const auto bin     = file.peek(file.size());
    const auto payload = std::string(bin.data(), bin.size());

    isLoading_ = true;
    emit loadingChanged();

    // First we need to create a new mxc URI
    // (i.e upload media to the Matrix content repository) for the new avatar.
    http::client()->upload(
      payload,
      mime.name().toStdString(),
      QFileInfo(fileName).fileName().toStdString(),
      [this,
       payload,
       mimetype = mime.name().toStdString(),
       size     = payload.size(),
       room_id  = roomid_.toStdString(),
       content = std::move(bin)](const mtx::responses::ContentURI &res, mtx::http::RequestErr err) {
          if (err) {
              nhlog::ui()->error("Failed to upload image: {}", *err);
              return;
          }

          if (isGlobalUserProfile()) {
              http::client()->set_avatar_url(res.content_uri, [this](mtx::http::RequestErr err) {
                  if (err) {
                      nhlog::ui()->error("Failed to set user avatar url: {}", *err);
                  }

                  isLoading_ = false;
                  emit loadingChanged();
                  getGlobalProfileData();
              });
          } else {
              // change room username
              mtx::events::state::Member member;
              member.display_name = cache::displayName(roomid_, userid_).toStdString();
              member.avatar_url   = res.content_uri;
              member.membership   = mtx::events::state::Membership::Join;

              updateRoomMemberState(std::move(member));
          }
      });
}

void
UserProfile::removeAvatar()
{
    if (!isGlobalUserProfile()) {
        // For room profiles, set the avatar URL to empty via room member state
        mtx::events::state::Member member;
        member.display_name = cache::displayName(roomid_, userid_).toStdString();
        member.avatar_url   = "";
        member.membership   = mtx::events::state::Membership::Join;
        updateRoomMemberState(std::move(member));
        return;
    }

    isLoading_ = true;
    emit loadingChanged();

    http::client()->set_avatar_url("", [this](mtx::http::RequestErr err) {
        if (err) {
            nhlog::ui()->error("Failed to remove user avatar: {}", *err);
            emit displayError(tr("Failed to remove avatar: %1")
                                .arg(QString::fromStdString(err->matrix_error.error)));
        }

        isLoading_ = false;
        emit loadingChanged();
        getGlobalProfileData();
    });
}

void
UserProfile::updateRoomMemberState(mtx::events::state::Member member)
{
    http::client()->send_state_event(roomid_.toStdString(),
                                     utils::localUser().toStdString(),
                                     member,
                                     [](mtx::responses::EventId, mtx::http::RequestErr err) {
                                         if (err)
                                             nhlog::net()->error(
                                               "Failed to update room member state: {}", *err);
                                     });
}

void
UserProfile::openGlobalProfile()
{
    emit manager->openGlobalUserProfile(userid_);
}
