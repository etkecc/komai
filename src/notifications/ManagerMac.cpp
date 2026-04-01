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
    const auto sender = notification.senderDisplayName;

    const auto room_id         = notification.roomId;
    const auto event_id        = notification.eventId;
    const auto notification_id = !notification.replacementEventId.isEmpty()
                                   ? notification.replacementEventId
                                   : (!event_id.isEmpty() ? event_id : room_id);
    const auto bodyText        = plainNotificationBody(notification);
    QString mediaMxcUrl        = notification.mediaMxcUrl;
    mediaMxcUrl.remove(QStringLiteral("mxc://"));

    if (notification.isEncrypted) {
        const QString messageInfo =
          (notification.isReply ? tr("%1 replied with an encrypted message")
                                : tr("%1 sent an encrypted message"))
            .arg(sender);
        objCxxPostNotification(room_name,
                               room_id,
                               event_id,
                               notification_id,
                               messageInfo,
                               "",
                               "",
                               notification.playSound);
    } else {
        const QString messageInfo =
          (notification.isReply ? tr("%1 replied to a message") : tr("%1 sent a message"))
            .arg(sender);
        if (allowShowingImages() && notification.hasInlineImage && !mediaMxcUrl.isEmpty())
            MxcImageProvider::download(
              mediaMxcUrl,
              QSize(200, 80),
              [this,
               room_name,
               room_id,
               event_id,
               notification_id,
               messageInfo,
               bodyText,
               playSound = notification.playSound](QString, QSize, QImage, QString imgPath) {
                  objCxxPostNotification(room_name,
                                         room_id,
                                         event_id,
                                         notification_id,
                                         messageInfo,
                                         bodyText,
                                         imgPath,
                                         playSound);
              });
        else
            objCxxPostNotification(room_name,
                                   room_id,
                                   event_id,
                                   notification_id,
                                   messageInfo,
                                   bodyText,
                                   "",
                                   notification.playSound);
    }
}
