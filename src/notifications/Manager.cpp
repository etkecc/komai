// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notifications/Manager.h"

#include <algorithm>

#include "settings/ui/facade/UserSettingsPage.h"

QString
NotificationsManager::trackedNotificationKey(const QString &roomId, const QString &eventId)
{
    return roomId + QChar(u'\x1f') + eventId;
}

void
NotificationsManager::rememberTrackedNotification(const QString &roomId, const QString &eventId)
{
    const auto key = trackedNotificationKey(roomId, eventId);
    if (trackedNotifications.contains(key))
        return;

    trackedNotifications.insert(key,
                                roomEventId{roomId, eventId, nextTrackedNotificationSequence_++});
}

void
NotificationsManager::forgetTrackedNotification(const QString &roomId, const QString &eventId)
{
    trackedNotifications.remove(trackedNotificationKey(roomId, eventId));
}

QVector<roomEventId>
NotificationsManager::trackedNotificationsForRoom(const QString &roomId) const
{
    QVector<roomEventId> matches;
    matches.reserve(trackedNotifications.size());

    for (const auto &entry : std::as_const(trackedNotifications)) {
        if (entry.roomId == roomId)
            matches.push_back(entry);
    }

    std::sort(
      matches.begin(), matches.end(), [](const roomEventId &left, const roomEventId &right) {
          return left.sequence < right.sequence;
      });

    return matches;
}

bool
NotificationsManager::allowShowingImages() const
{
    auto show = UserSettings::instance()->timelineMediaImageDisplay();

    switch (show) {
    case UserSettings::ShowImage::Always:
        return true;
    case UserSettings::ShowImage::OnlyPrivate:
        return false;
    case UserSettings::ShowImage::Never:
    default:
        return false;
    }
}

QString
NotificationsManager::getMessageTemplate(const komai::NotificationPayload &notification)
{
    const auto sender               = notification.senderDisplayName;
    const auto messageContentPolicy = UserSettings::instance()->notificationsMessageContentPolicy();

    if (messageContentPolicy == UserSettings::NotificationMessageContentPolicy::Never)
        return tr("%1 sent a message").arg(sender);

    if (notification.isEncrypted) {
        return tr("%1 sent an encrypted message").arg(sender);
    }

    if (notification.containsSpoiler) {
        // Because we skip the %2 here, this might cause a warning in some cases.
        if (notification.isEmote) {
            return QStringLiteral("* %1 spoils something.").arg(sender);
        } else if (notification.isReply) {
            return tr("%1 replied with a spoiler.",
                      "Format a reply in a notification. %1 is the sender.")
              .arg(sender);
        } else {
            return QStringLiteral("%1 sent a spoiler.").arg(sender);
        }
    } else {
        if (notification.isEmote) {
            return QStringLiteral("* %1 %2").arg(sender);
        } else if (notification.isReply) {
            return tr("%1 replied: %2",
                      "Format a reply in a notification. %1 is the sender, %2 the message")
              .arg(sender);
        } else {
            return QStringLiteral("%1: %2").arg(sender);
        }
    }
}

QString
NotificationsManager::plainNotificationBody(const komai::NotificationPayload &notification)
{
    if (notification.containsSpoiler)
        return tr("Message contains spoiler.");

    return notification.plainBody;
}

QString
NotificationsManager::formattedNotificationBody(const komai::NotificationPayload &notification)
{
    if (notification.containsSpoiler)
        return tr("Message contains spoiler.");

    return notification.formattedBody.isEmpty() ? notification.plainBody
                                                : notification.formattedBody;
}

void
NotificationsManager::removeNotifications(const QString &roomId_,
                                          const std::vector<QString> &eventIds)
{
    std::vector<roomEventId> matches;
    const auto tracked = trackedNotificationsForRoom(roomId_);
    matches.reserve(static_cast<size_t>(tracked.size()));

    for (const auto &entry : tracked) {
        if (eventIds.empty() ||
            std::find(eventIds.begin(), eventIds.end(), entry.eventId) != eventIds.end()) {
            matches.push_back(entry);
        }
    }

    for (const auto &entry : matches)
        removeNotification(entry.roomId, entry.eventId);
}

void
NotificationsManager::reconcileRoomNotifications(const QString &roomId, int keepNewestCount)
{
    if (roomId.isEmpty())
        return;

    const auto tracked          = trackedNotificationsForRoom(roomId);
    const auto clampedKeepCount = std::max(keepNewestCount, 0);
    if (tracked.size() <= clampedKeepCount)
        return;

    const auto removeCount = tracked.size() - clampedKeepCount;
    for (int index = 0; index < removeCount; ++index) {
        const auto &entry = tracked.at(index);
        removeNotification(entry.roomId, entry.eventId);
    }
}

#include "moc_Manager.cpp"
