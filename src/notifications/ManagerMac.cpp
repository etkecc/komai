// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Manager.h"

#include <QCoreApplication>

#include "providers/MxcImageProvider.h"

NotificationsManager::NotificationsManager(QObject *parent)
  : QObject(parent)
{
    // Putting these here to pass along since I'm not sure how
    // our translate step interacts with .mm files
    respondStr  = QObject::tr("Respond");
    sendStr     = QObject::tr("Send");
    placeholder = QObject::tr("Write a message...");

    connect(
      this,
      &NotificationsManager::systemPostNotificationCb,
      this,
      [this](const QString &room_id,
             const QString &event_id,
             const QString &roomName,
             const QString &text,
             const QImage &) {
          const auto notification_id = event_id.isEmpty() ? room_id : event_id;
          rememberTrackedNotification(room_id, event_id);
          objCxxPostNotification(roomName,
                                 room_id,
                                 event_id,
                                 notification_id,
                                 text,
                                 /*const QString &informativeText*/ "",
                                 "",
                                 true);
      },
      Qt::QueuedConnection);
}

void
NotificationsManager::postNotification(const komai::NotificationPayload &notification,
                                       const QImage &icon)
{
    Q_UNUSED(icon)

    const auto room_name =
      notification.roomName.isEmpty() ? notification.roomId : notification.roomName;
    const auto sender          = notification.senderDisplayName;
    const auto room_id         = notification.roomId;
    const auto target_event_id = komai::notificationTargetEventId(notification);
    const auto notification_id = komai::notificationStableId(notification);
    const auto bodyText        = plainNotificationBody(notification);
    QString mediaMxcUrl        = notification.mediaMxcUrl;
    mediaMxcUrl.remove(QStringLiteral("mxc://"));
    auto postResolvedNotification =
      [this, room_name, room_id, target_event_id, notification_id](const QString &subtitle,
                                                                   const QString &informativeText,
                                                                   const QString &bodyImagePath,
                                                                   bool playSound) {
          rememberTrackedNotification(room_id, target_event_id);
          objCxxPostNotification(room_name,
                                 room_id,
                                 target_event_id,
                                 notification_id,
                                 subtitle,
                                 informativeText,
                                 bodyImagePath,
                                 playSound);
      };

    if (notification.isEncrypted) {
        const QString messageInfo =
          (notification.isReply ? tr("%1 replied with an encrypted message")
                                : tr("%1 sent an encrypted message"))
            .arg(sender);
        postResolvedNotification(messageInfo, "", "", notification.playSound);
    } else {
        const QString messageInfo =
          (notification.isReply ? tr("%1 replied to a message") : tr("%1 sent a message"))
            .arg(sender);
        if (allowShowingImages() && notification.hasInlineImage && !mediaMxcUrl.isEmpty())
            MxcImageProvider::download(
              mediaMxcUrl,
              QSize(200, 80),
              [postResolvedNotification, messageInfo, bodyText, playSound = notification.playSound](
                QString, QSize, QImage, QString imgPath) {
                  postResolvedNotification(messageInfo, bodyText, imgPath, playSound);
              });
        else
            postResolvedNotification(messageInfo, bodyText, "", notification.playSound);
    }
}

void
NotificationsManager::postCallNotification(const QString &roomId,
                                           const QString &eventId,
                                           const QString &roomName,
                                           bool isRing,
                                           bool canDecline,
                                           const QImage &icon)
{
    Q_UNUSED(icon)
    Q_UNUSED(canDecline)
    // Element Call is not built on macOS yet (QtWebEngine packaging is handled in
    // a later milestone), so this is never reached on this platform. Show a
    // plain notice (clicking opens the room) and track it as a call notification
    // so removeCallNotificationsForRoom can find it.
    const auto room_name = roomName.isEmpty() ? roomId : roomName;
    rememberTrackedNotification(roomId, eventId);
    rememberCallNotification(roomId, eventId, canDecline);
    objCxxPostNotification(room_name,
                           roomId,
                           eventId,
                           eventId,
                           isRing ? tr("Incoming call") : tr("Started a call"),
                           QString(),
                           QString(),
                           /*playSound=*/false);
}
