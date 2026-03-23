// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SharedLogic.h"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "config/komai.h"
#include "matrix/MatrixClient.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

namespace {

RoomlistModel *
currentRoomlistModel()
{
    auto *chatPage = ChatPage::instance();
    if (!chatPage || !chatPage->timelineManager())
        return nullptr;
    return chatPage->timelineManager()->rooms();
}

} // namespace

namespace komai::ipc {

// -- app --

QString
apiVersion()
{
    return apiVersionNumber.toString();
}

QString
appVersion()
{
    return komai::version;
}

// -- rooms --

QJsonObject
RoomInfo::toJson() const
{
    return {
      {QStringLiteral("id"), roomId},
      {QStringLiteral("alias"), alias},
      {QStringLiteral("name"), name},
      {QStringLiteral("avatarUrl"), avatarUrl},
      {QStringLiteral("unreadNotifications"), unreadNotifications},
    };
}

QVector<RoomInfo>
roomList()
{
    auto *rl = currentRoomlistModel();
    if (!rl)
        return {};

    QVector<RoomInfo> result;
    result.reserve(static_cast<int>(rl->roomids.size()));

    for (const auto &roomId : rl->roomids) {
        if (rl->invites.contains(roomId) || rl->previewedRooms.contains(roomId))
            continue;

        const auto aliases =
          cache::getStateEvent<mtx::events::state::CanonicalAlias>(roomId.toStdString());
        QString alias;
        if (aliases.has_value()) {
            const auto &val = aliases.value().content;
            if (!val.alias.empty())
                alias = QString::fromStdString(val.alias);
            else if (val.alt_aliases.size() > 0)
                alias = QString::fromStdString(val.alt_aliases.front());
        }

        QString roomName;
        QString roomAvatar;
        int notificationCount = 0;

        if (rl->models.contains(roomId)) {
            const auto &room = rl->models.value(roomId);
            if (room.isNull())
                continue;

            roomName          = room->plainRoomName();
            roomAvatar        = room->roomAvatarUrl();
            notificationCount = room->notificationCount();
        } else if (rl->cachedJoinedRooms_.contains(roomId)) {
            const auto roomInfo = rl->cachedJoinedRooms_.value(roomId);
            roomName            = QString::fromStdString(roomInfo.name);
            roomAvatar          = QString::fromStdString(roomInfo.avatar_url);
            notificationCount   = static_cast<int>(roomInfo.notification_count);
        } else {
            continue;
        }

        if (roomAvatar.isEmpty())
            roomAvatar = cache::roomAvatarUrl(roomId.toStdString());

        result.push_back({roomId, alias, roomName, roomAvatar, notificationCount});
    }

    return result;
}

void
activateRoom(const QString &roomIdOrAlias)
{
    MainWindow::instance()->show();
    MainWindow::instance()->raise();
    if (auto *rl = currentRoomlistModel())
        rl->setCurrentRoom(roomIdOrAlias);
}

void
joinRoom(const QString &roomIdOrAlias)
{
    MainWindow::instance()->show();
    MainWindow::instance()->raise();
    ChatPage::instance()->joinRoom(roomIdOrAlias);
}

void
newDirectChat(const QString &userId)
{
    MainWindow::instance()->show();
    MainWindow::instance()->raise();
    ChatPage::instance()->startChat(userId);
}

// -- user --

QString
userId()
{
    return QString::fromStdString(http::client()->user_id().to_string());
}

QString
homeserverUrl()
{
    return QString::fromStdString(http::client()->server_url());
}

QString
deviceId()
{
    return QString::fromStdString(http::client()->device_id());
}

QString
statusMessage()
{
    return ChatPage::instance()->status();
}

void
setStatusMessage(const QString &message)
{
    ChatPage::instance()->setStatus(message);
}

// -- settings.ui --

QString
uiTheme()
{
    const auto settings = UserSettings::instance();
    if (!settings)
        return {};

    return settings->uiThemeSlug();
}

void
setUiTheme(const QString &theme)
{
    const auto settings = UserSettings::instance();
    if (!settings)
        return;

    settings->setUiThemeSlug(theme);
}

// -- media --

void
mediaFetch(const QString &mxcUri, MediaFetchCallback callback)
{
    MainWindow::instance()->imageProvider()->download(
      QString(mxcUri).remove(QStringLiteral("mxc://")),
      {96, 96},
      [callback](const QString &, const QSize &, const QImage &image, const QString &) {
          if (callback)
              callback(image);
      },
      true);
}

} // namespace komai::ipc
