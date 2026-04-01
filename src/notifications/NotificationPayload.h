// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace komai {

// Detached notification payload used by platform notification backends.
// This keeps the UI/platform layer independent from mtxclient-era response types.
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

} // namespace komai
