// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notifications/Manager.h"

#include "settings/ui/facade/UserSettingsPage.h"

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
    for (const auto &[roomId, eventId] : std::as_const(this->notificationIds)) {
        if (roomId != roomId_)
            continue;
        if (eventIds.empty() ||
            std::find(eventIds.begin(), eventIds.end(), eventId) != eventIds.end()) {
            removeNotification(roomId, eventId);
        }
    }
}

#include "moc_Manager.cpp"
