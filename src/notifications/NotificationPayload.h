// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace komai {

// Detached notification payload used by platform notification backends.
// This keeps the UI/platform layer independent from backend-specific response types.
struct NotificationPayload
{
    QString roomId;
    QString eventId;
    QString replacementEventId;
    QString roomName;
    QString senderDisplayName;
    QString plainBody;
    QString formattedBody;
    QString mediaMxcUrl;

    bool isReply         = false;
    bool isEmote         = false;
    bool isEncrypted     = false;
    bool containsSpoiler = false;
    bool hasInlineImage  = false;
    bool playSound       = false;
};

// Interaction should target the visible timeline event. For edits that means
// the replaced/original event, not the edit event wrapper itself.
inline QString
notificationTargetEventId(const NotificationPayload &notification)
{
    return notification.replacementEventId.isEmpty() ? notification.eventId
                                                     : notification.replacementEventId;
}

// Platform notification backends need a stable identifier even when there is
// no event id yet (for example stripped invites), so fall back to the room id.
inline QString
notificationStableId(const NotificationPayload &notification)
{
    const auto targetEventId = notificationTargetEventId(notification);
    return targetEventId.isEmpty() ? notification.roomId : targetEventId;
}

} // namespace komai
