// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QMap>
#include <QObject>
#include <QString>

#include "notifications/NotificationPayload.h"

#if defined(KOMAI_DBUS_SYS)
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusInterface>
#endif

struct roomEventId
{
    QString roomId;
    QString eventId;
    quint64 sequence = 0;
};

inline bool
operator==(const roomEventId &a, const roomEventId &b)
{
    return a.roomId == b.roomId && a.eventId == b.eventId;
}

class NotificationsManager final : public QObject
{
    Q_OBJECT
public:
    NotificationsManager(QObject *parent = nullptr);

    void postNotification(const komai::NotificationPayload &notification, const QImage &icon);

    // Post a desktop notification for an incoming MatrixRTC (Element Call) call.
    // `isRing` is an addressed 1:1 invite; otherwise it is a silent group-call
    // notice. The notification carries a "Join" action (and, when `canDecline`,
    // a "Decline" action); activating it or clicking the body joins the call.
    void postCallNotification(const QString &roomId,
                              const QString &eventId,
                              const QString &roomName,
                              bool isRing,
                              bool canDecline,
                              const QImage &icon);

    void removeNotification(const QString &roomId, const QString &eventId);
    // Close all call notifications currently shown for the room (leaves ordinary
    // message notifications in place).
    void removeCallNotificationsForRoom(const QString &roomId);
    void reconcileRoomNotifications(const QString &roomId, int keepNewestCount);

signals:
    void notificationClicked(const QString roomId, const QString eventId);
    void sendNotificationReply(const QString roomId, const QString eventId, const QString body);
    // An incoming-call notification's Join action (or body click) was activated.
    void callJoinRequested(const QString roomId);
    // An incoming-call notification's Decline action was activated.
    void callDeclineRequested(const QString roomId, const QString eventId);
    void systemPostNotificationCb(const QString &room_id,
                                  const QString &event_id,
                                  const QString &roomName,
                                  const QString &text,
                                  const QImage &icon);

public slots:
    void removeNotifications(const QString &roomId, const std::vector<QString> &eventId);

#if defined(KOMAI_DBUS_SYS)
public:
    void closeNotifications(QString roomId);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    void closeAllNotifications();
#endif

private:
    QDBusInterface dbus;

    void systemPostNotification(const QString &room_id,
                                const QString &event_id,
                                const QString &roomName,
                                const QString &text,
                                const QImage &icon);
    void closeNotification(uint id);
    const bool hasMarkup_;
    const bool hasImages_;
#endif

#if defined(Q_OS_MACOS)
private:
    // Objective-C(++) doesn't like to do lots of regular C++, so the actual notification
    // posting is split out
    void objCxxPostNotification(const QString room_name,
                                const QString room_id,
                                const QString event_id,
                                const QString notification_id,
                                const QString subtitle,
                                const QString informativeText,
                                const QString bodyImagePath,
                                const bool playSound);

    QString respondStr;
    QString sendStr;
    QString placeholder;

public:
    static void attachToMacNotifCenter();
#endif

#if defined(Q_OS_WINDOWS)
private:
    void systemPostNotification(const QString &roomid,
                                const QString &eventid,
                                const QString &line1,
                                const QString &line2,
                                const QString &iconPath,
                                const QString &bodyImagePath);

    QMap<QString, qint64> windowsNotificationIds;
#endif

    // these slots are platform specific (D-Bus only)
    // but Qt slot declarations can not be inside an ifdef!
private slots:
    void actionInvoked(uint id, QString action);
    void activationToken(uint id, QString action);
    void notificationClosed(uint id, uint reason);
    void notificationReplied(uint id, QString reply);

private:
    static QString trackedNotificationKey(const QString &roomId, const QString &eventId);
    QVector<roomEventId> trackedNotificationsForRoom(const QString &roomId) const;
    void rememberTrackedNotification(const QString &roomId, const QString &eventId);
    void forgetTrackedNotification(const QString &roomId, const QString &eventId);

    // Mark a tracked notification as an incoming-call one (so action handlers
    // route Join/Decline, and removeCallNotificationsForRoom can find them).
    void rememberCallNotification(const QString &roomId, const QString &eventId, bool canDecline);
    void forgetCallNotification(const QString &roomId, const QString &eventId);
    bool isCallNotification(const QString &roomId, const QString &eventId) const;

    QString getMessageTemplate(const komai::NotificationPayload &notification);
    QString plainNotificationBody(const komai::NotificationPayload &notification);
    QString formattedNotificationBody(const komai::NotificationPayload &notification);
    bool allowShowingImages() const;

    // Cross-platform tracking for "remove all notifications in this room" semantics.
    QMap<QString, roomEventId> trackedNotifications;
    quint64 nextTrackedNotificationSequence_ = 1;

    // Tracked-notification keys that are incoming-call notifications, mapped to
    // whether they offer a Decline action. Used to route Join/Decline activation
    // and to close just the call notifications for a room.
    QMap<QString, bool> callNotifications_;

    // Linux D-Bus notification id to (room ID, event ID).
    QMap<uint, roomEventId> notificationIds;
};
