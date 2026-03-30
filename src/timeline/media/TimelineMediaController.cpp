// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineMediaController.h"

#include "chat/ChatPage.h"
#include "logging/Logging.h"

timeline::media::TimelineMediaController::TimelineMediaController(QString roomId,
                                                                  MediaCachedCallback mediaCached)
  : roomId_(std::move(roomId))
  , mediaCached_(std::move(mediaCached))
{
}

void
timeline::media::TimelineMediaController::openMedia(const QString &eventId) const
{
    nhlog::ui()->warn("Legacy timeline media open is not migrated to matrix-sdk yet (room='{}', "
                      "event='{}')",
                      roomId_.toStdString(),
                      eventId.toStdString());
    emit ChatPage::instance()->showNotification(
      QObject::tr("Legacy timeline media opening is not migrated yet."));
}

bool
timeline::media::TimelineMediaController::saveMedia(const QString &eventId) const
{
    nhlog::ui()->warn("Legacy timeline media save is not migrated to matrix-sdk yet (room='{}', "
                      "event='{}')",
                      roomId_.toStdString(),
                      eventId.toStdString());
    emit ChatPage::instance()->showNotification(
      QObject::tr("Legacy timeline media saving is not migrated yet."));
    return false;
}

bool
timeline::media::TimelineMediaController::copyMedia(const QString &eventId) const
{
    nhlog::ui()->warn("Legacy timeline media copy is not migrated to matrix-sdk yet (room='{}', "
                      "event='{}')",
                      roomId_.toStdString(),
                      eventId.toStdString());
    emit ChatPage::instance()->showNotification(
      QObject::tr("Legacy timeline media copying is not migrated yet."));
    return false;
}

void
timeline::media::TimelineMediaController::cacheMedia(
  const QString &eventId,
  const std::function<void(const QString &)> &callback) const
{
    Q_UNUSED(callback);
    nhlog::ui()->warn("Legacy timeline media cache is not migrated to matrix-sdk yet (room='{}', "
                      "event='{}')",
                      roomId_.toStdString(),
                      eventId.toStdString());
}
