// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSummary.h"

#include <QMetaType>

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
        if (const auto *window = MainWindow::instance();
            window && window->matrixBackendHandleId()) {
            const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
            QString error;
            const auto roomId   = QString::fromStdString(roomIdOrAlias);
            const auto settings = komai::MatrixBackendRuntimeService::fetchRoomSettings(
              context, window->matrixBackendHandleId(), roomId, &error);

            if (settings.has_value()) {
                this->room = LoadedRoomSummary{
                  .roomId      = roomId,
                  .name        = settings->roomName,
                  .topic       = settings->roomTopic,
                  .avatarUrl   = komai::matrix::normalizeMxcUri(settings->roomAvatarUrl),
                  .memberCount = static_cast<int>(settings->memberCount),
                  .isInvite    = false,
                  .isSpace     = false,
                  .isKnockOnly = settings->joinRule == QLatin1String("knock") ||
                                 settings->joinRule == QLatin1String("knock_restricted"),
                };
                loaded_ = true;
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
    const auto name = room ? room->name : QString::fromStdString(roomIdOrAlias);
    return utils::replaceEmoji(name.toHtmlEscaped());
}
QString
RoomSummary::roomTopic() const
{
    if (!room)
        return {};

    return utils::replaceEmoji(utils::linkifyMessage(
      room->topic.toHtmlEscaped().replace(QLatin1String("\n"), QLatin1String("<br>"))));
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
