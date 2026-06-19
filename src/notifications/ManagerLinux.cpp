// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notifications/Manager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <QImage>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QTextDocumentFragment>

#include "dbus/Api.h"
#include "logging/Logging.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include <functional>

NotificationsManager::NotificationsManager(QObject *parent)
  : QObject(parent)
  , dbus(QStringLiteral("org.freedesktop.Notifications"),
         QStringLiteral("/org/freedesktop/Notifications"),
         QStringLiteral("org.freedesktop.Notifications"),
         QDBusConnection::sessionBus(),
         this)
  , hasMarkup_{std::invoke([this]() -> bool {
      auto caps = dbus.call("GetCapabilities").arguments();
      for (const auto &x : std::as_const(caps))
          if (x.toStringList().contains("body-markup"))
              return true;
      return false;
  })}
  , hasImages_{std::invoke([this]() -> bool {
      auto caps = dbus.call("GetCapabilities").arguments();
      for (const auto &x : std::as_const(caps))
          if (x.toStringList().contains("body-images"))
              return true;
      return false;
  })}
{
    qDBusRegisterMetaType<QImage>();

    // clang-format off
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("/org/freedesktop/Notifications"),
                                          QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("ActionInvoked"),
                                          this,
                                          SLOT(actionInvoked(uint,QString)));
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("/org/freedesktop/Notifications"),
                                          QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("ActivationToken"),
                                          this,
                                          SLOT(activationToken(uint,QString)));
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("/org/freedesktop/Notifications"),
                                          QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("NotificationClosed"),
                                          this,
                                          SLOT(notificationClosed(uint,uint)));
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("/org/freedesktop/Notifications"),
                                          QStringLiteral("org.freedesktop.Notifications"),
                                          QStringLiteral("NotificationReplied"),
                                          this,
                                          SLOT(notificationReplied(uint,QString)));
    // clang-format on

    connect(this,
            &NotificationsManager::systemPostNotificationCb,
            this,
            &NotificationsManager::systemPostNotification,
            Qt::QueuedConnection);
}

void
NotificationsManager::postNotification(const komai::NotificationPayload &notification,
                                       const QImage &icon)
{
    const auto room_id         = notification.roomId;
    const auto target_event_id = komai::notificationTargetEventId(notification);
    const auto room_name       = notification.roomName.isEmpty() ? room_id : notification.roomName;
    QString formattedBody      = formattedNotificationBody(notification);
    QString mediaMxcUrl        = notification.mediaMxcUrl;
    mediaMxcUrl.remove(QStringLiteral("mxc://"));

    auto postNotif = [this, room_id, target_event_id, room_name, icon](QString text) {
        emit systemPostNotificationCb(room_id, target_event_id, room_name, text, icon);
    };

    QString template_ = getMessageTemplate(notification);
    if (notification.isEncrypted || !template_.contains("%2")) {
        postNotif(template_);
        return;
    }

    if (hasMarkup_) {
        if (hasImages_ && allowShowingImages() && notification.hasInlineImage &&
            !mediaMxcUrl.isEmpty()) {
            // The `alt` attribute is HTML; the body is already Pango markup
            // (containing `<b>`, `<a>`, ...), so use the plain variant here
            // and escape it.
            const QString altText = plainNotificationBody(notification).toHtmlEscaped();
            // Fit-into-200x80 with aspect ratio preserved. Keeps the popup
            // compact; `crop=false` avoids slicing portrait sources through
            // the vertical middle.
            MxcImageProvider::download(
              mediaMxcUrl,
              QSize(200, 80),
              [postNotif, formattedBody, altText, template_](
                QString, QSize, QImage, QString imgPath) {
                  if (imgPath.isEmpty())
                      postNotif(template_.arg(formattedBody));
                  else
                      postNotif(template_.arg(QStringLiteral("<br><img src=\"file:///") % imgPath %
                                              "\" alt=\"" % altText % "\">"));
              },
              /*crop=*/false);
            return;
        }

        postNotif(template_.arg(formattedBody));
        return;
    }

    postNotif(template_.arg(plainNotificationBody(notification)));
}

/**
 * This function is based on code from
 * https://github.com/rohieb/StratumsphereTrayIcon
 * Copyright (C) 2012 Roland Hieber <rohieb@rohieb.name>
 * Licensed under the GNU General Public License, version 3
 */
void
NotificationsManager::systemPostNotification(const QString &room_id,
                                             const QString &event_id,
                                             const QString &roomName,
                                             const QString &text,
                                             const QImage &icon)
{
    QVariantMap hints;
    hints[QStringLiteral("image-data")]    = icon;
    hints[QStringLiteral("desktop-entry")] = "komai";
    hints[QStringLiteral("category")]      = "im.received";
    hints[QStringLiteral("sound-name")]    = "message-new-instant";

    if (auto profile = UserSettings::instance()->profile(); !profile.isEmpty())
        hints[QStringLiteral("x-kde-origin-name")] = profile;

    uint replace_id = 0;
    if (!event_id.isEmpty()) {
        for (auto elem = notificationIds.begin(); elem != notificationIds.end(); ++elem) {
            if (elem.value().roomId != room_id)
                continue;

            if (elem.value().eventId == event_id) {
                replace_id = elem.key();
                break;
            }
        }
    }

    QList<QVariant> argumentList;
    argumentList << "Komai";          // app_name
    argumentList << (uint)replace_id; // replace_id
    argumentList << "";               // app_icon
    argumentList << roomName;         // summary
    argumentList << text;             // body

    QStringList actions;
    actions << QStringLiteral("default") << tr("Open");
    if (!room_id.isEmpty() && !event_id.isEmpty()) {
        actions << QStringLiteral("inline-reply") << tr("Reply");
    }
    argumentList << actions; // actions
    argumentList << hints;   // hints
    argumentList << (int)-1; // timeout in ms

    QDBusPendingCall call = dbus.asyncCallWithArgumentList(QStringLiteral("Notify"), argumentList);
    auto watcher          = new QDBusPendingCallWatcher{call, this};
    connect(
      watcher, &QDBusPendingCallWatcher::finished, this, [watcher, this, room_id, event_id]() {
          if (watcher->reply().type() == QDBusMessage::ErrorMessage)
              qDebug() << "D-Bus Error:" << watcher->reply().errorMessage();
          else {
              notificationIds[watcher->reply().arguments().first().toUInt()] =
                roomEventId{room_id, event_id};
              rememberTrackedNotification(room_id, event_id);
          }
          watcher->deleteLater();
      });
}

void
NotificationsManager::postCallNotification(const QString &roomId,
                                           const QString &eventId,
                                           const QString &roomName,
                                           bool isRing,
                                           bool canDecline,
                                           const QImage &icon)
{
    QVariantMap hints;
    hints[QStringLiteral("image-data")]    = icon;
    hints[QStringLiteral("desktop-entry")] = "komai";
    hints[QStringLiteral("category")] = isRing ? "x-komai.call.incoming" : "x-komai.call.group";
    // A ring is urgent and should stay until answered/declined; we play our own
    // ringtone, so suppress the notification server's sound to avoid doubling up.
    hints[QStringLiteral("urgency")]        = isRing ? uchar(2) : uchar(1);
    hints[QStringLiteral("suppress-sound")] = true;

    if (auto profile = UserSettings::instance()->profile(); !profile.isEmpty())
        hints[QStringLiteral("x-kde-origin-name")] = profile;

    uint replace_id = 0;
    for (auto elem = notificationIds.begin(); elem != notificationIds.end(); ++elem) {
        if (elem.value().roomId == roomId && elem.value().eventId == eventId) {
            replace_id = elem.key();
            break;
        }
    }

    const QString summary = roomName.isEmpty() ? roomId : roomName;
    const QString body    = isRing ? tr("Incoming call") : tr("Started a call");

    // "default" makes clicking the popup body join; "join" is the explicit
    // button. A ring also offers "decline".
    QStringList actions;
    actions << QStringLiteral("default") << tr("Join");
    actions << QStringLiteral("join") << tr("Join");
    if (canDecline)
        actions << QStringLiteral("decline") << tr("Decline");

    QList<QVariant> argumentList;
    argumentList << QStringLiteral("Komai"); // app_name
    argumentList << replace_id;              // replace_id
    argumentList << QString();               // app_icon
    argumentList << summary;                 // summary
    argumentList << body;                    // body
    argumentList << actions;                 // actions
    argumentList << hints;                   // hints
    // A ring stays put (we close it on answer/decline/expiry); a silent group
    // notice may fade on the server's default timeout.
    argumentList << (isRing ? 0 : -1); // timeout in ms

    QDBusPendingCall call = dbus.asyncCallWithArgumentList(QStringLiteral("Notify"), argumentList);
    auto watcher          = new QDBusPendingCallWatcher{call, this};
    connect(watcher,
            &QDBusPendingCallWatcher::finished,
            this,
            [watcher, this, roomId, eventId, canDecline]() {
                if (watcher->reply().type() == QDBusMessage::ErrorMessage)
                    qDebug() << "D-Bus Error:" << watcher->reply().errorMessage();
                else {
                    notificationIds[watcher->reply().arguments().first().toUInt()] =
                      roomEventId{roomId, eventId};
                    rememberTrackedNotification(roomId, eventId);
                    rememberCallNotification(roomId, eventId, canDecline);
                }
                watcher->deleteLater();
            });
}

void
NotificationsManager::closeNotification(uint id)
{
    auto call    = dbus.asyncCall(QStringLiteral("CloseNotification"), (uint)id); // replace_id
    auto watcher = new QDBusPendingCallWatcher{call, this};
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher]() {
        if (watcher->reply().type() == QDBusMessage::ErrorMessage) {
            qDebug() << "D-Bus Error:" << watcher->reply().errorMessage();
        };
        watcher->deleteLater();
    });
}

void
NotificationsManager::removeNotification(const QString &roomId, const QString &eventId)
{
    for (auto elem = notificationIds.begin(); elem != notificationIds.end(); ++elem) {
        if (elem.value().roomId != roomId || elem.value().eventId != eventId)
            continue;

        const auto notificationId = elem.key();
        notificationIds.erase(elem);
        forgetTrackedNotification(roomId, eventId);
        closeNotification(notificationId);
        return;
    }

    forgetTrackedNotification(roomId, eventId);
}

void
NotificationsManager::actionInvoked(uint id, QString action)
{
    if (notificationIds.contains(id)) {
        roomEventId idEntry = notificationIds[id];
        const bool isCall   = isCallNotification(idEntry.roomId, idEntry.eventId);
        if (action == QLatin1String("decline")) {
            emit callDeclineRequested(idEntry.roomId, idEntry.eventId);
        } else if (action == QLatin1String("join") ||
                   (isCall && action == QLatin1String("default"))) {
            emit callJoinRequested(idEntry.roomId);
        } else if (action == QLatin1String("default")) {
            emit notificationClicked(idEntry.roomId, idEntry.eventId);
        }
    }
}

// receive a wayland activation token from the notification manager
void
NotificationsManager::activationToken(uint, QString action)
{
    komai::logging::net()->debug("Got activation token for notification");
    qputenv("XDG_ACTIVATION_TOKEN", action.toUtf8());
}

void
NotificationsManager::notificationReplied(uint id, QString reply)
{
    if (notificationIds.contains(id)) {
        roomEventId idEntry = notificationIds[id];
        emit sendNotificationReply(idEntry.roomId, idEntry.eventId, reply);
    }
}

void
NotificationsManager::notificationClosed(uint id, uint reason)
{
    Q_UNUSED(reason);
    if (notificationIds.contains(id)) {
        const auto entry = notificationIds[id];
        forgetTrackedNotification(entry.roomId, entry.eventId);
    }
    notificationIds.remove(id);
}

void
NotificationsManager::closeAllNotifications()
{
    const auto ids = notificationIds.keys();
    for (const auto &id : ids) {
        closeNotification(id);
        notificationIds.remove(id);
    }
}
