// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSummary.h"

#include <QMetaType>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

RoomSummary::RoomSummary(std::string roomIdOrAlias_,
                         std::vector<std::string> vias_,
                         QString r_,
                         QObject *p)
  : QObject(p)
  , roomIdOrAlias(std::move(roomIdOrAlias_))
  , vias(std::move(vias_))
  , reason_(std::move(r_))
{
    if (roomIdOrAlias.empty())
        return;

    if (roomIdOrAlias[0] == '!') {
        auto temp = cache::singleRoomInfo(roomIdOrAlias);

        if (temp.member_count) {
            mtx::responses::PublicRoom newInfo{};
            // newInfo.aliases;
            // newInfo.canonical_alias = "";
            newInfo.name               = temp.name;
            newInfo.room_id            = roomIdOrAlias;
            newInfo.topic              = temp.topic;
            newInfo.num_joined_members = temp.member_count;
            // newInfo.world_readable;
            newInfo.guest_can_join = temp.guest_access;
            newInfo.avatar_url     = temp.avatar_url;

            newInfo.join_rule    = temp.join_rule;
            newInfo.room_type    = temp.is_space ? mtx::events::state::room_type::space : "";
            newInfo.room_version = temp.version;
            newInfo.membership   = mtx::events::state::Membership::Join;
            // newInfo.encryption;

            this->room = std::move(newInfo);
            loaded_    = true;
            return;
        }

        if (const auto *window = MainWindow::instance();
            window && window->matrixBackendHandleId()) {
            QString error;
            const auto roomId   = QString::fromStdString(roomIdOrAlias);
            const auto settings = komai::MatrixBackendRuntimeService::fetchRoomSettings(
              window->matrixBackendHandleId(), roomId, &error);

            if (settings.has_value()) {
                mtx::responses::PublicRoom newInfo{};
                newInfo.name               = settings->roomName.toStdString();
                newInfo.room_id            = roomIdOrAlias;
                newInfo.topic              = settings->roomTopic.toStdString();
                newInfo.num_joined_members = static_cast<uint64_t>(settings->memberCount);
                newInfo.avatar_url =
                  komai::matrix::normalizeMxcUri(settings->roomAvatarUrl).toStdString();
                newInfo.room_version = settings->roomVersion.toStdString();
                newInfo.join_rule =
                  mtx::events::state::stringToJoinRule(settings->joinRule.toStdString());
                newInfo.guest_can_join = settings->guestAccess;
                newInfo.membership     = mtx::events::state::Membership::Join;
                this->room             = std::move(newInfo);
                loaded_                = true;
                return;
            }

            if (!error.isEmpty()) {
                nhlog::ui()->warn("Failed to fetch runtime room summary for '{}': {}",
                                  roomIdOrAlias,
                                  error.toStdString());
            }
        }
    }

    loaded_ = true;
    emit loaded();
}

QString
RoomSummary::roomName() const
{
    return utils::replaceEmoji(
      QString::fromStdString(room ? room->name : roomIdOrAlias).toHtmlEscaped());
}
QString
RoomSummary::roomTopic() const
{
    return room ? utils::replaceEmoji(
                    utils::linkifyMessage(QString::fromStdString(room->topic)
                                            .toHtmlEscaped()
                                            .replace(QLatin1String("\n"), QLatin1String("<br>"))))
                : "";
}

void
RoomSummary::join()
{
    if (isKnockOnly())
        ChatPage::instance()->knockRoom(
          QString::fromStdString(roomIdOrAlias), vias, reason_, false, false);
    else
        ChatPage::instance()->joinRoomVia(roomIdOrAlias, vias, false, reason_);
}

void
RoomSummary::promptJoin()
{
    if (isKnockOnly())
        ChatPage::instance()->knockRoom(
          QString::fromStdString(roomIdOrAlias), vias, reason_, false, true);
    else
        ChatPage::instance()->joinRoomVia(roomIdOrAlias, vias, true, reason_);
}

#include "moc_RoomSummary.cpp"
