// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>

#include "notifications/NotificationPayload.h"

namespace {

bool
expect(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testDefaultsToEventIdWhenNoReplacementExists()
{
    komai::NotificationPayload notification{};
    notification.roomId  = QStringLiteral("!room:example.com");
    notification.eventId = QStringLiteral("$event");

    bool ok = true;
    ok &= expect(komai::notificationTargetEventId(notification) == QStringLiteral("$event"),
                 "target event id defaults to event id");
    ok &= expect(komai::notificationStableId(notification) == QStringLiteral("$event"),
                 "stable id defaults to event id");
    return ok;
}

bool
testReplacementEventBecomesInteractionTarget()
{
    komai::NotificationPayload notification{};
    notification.roomId             = QStringLiteral("!room:example.com");
    notification.eventId            = QStringLiteral("$edit");
    notification.replacementEventId = QStringLiteral("$original");

    bool ok = true;
    ok &= expect(komai::notificationTargetEventId(notification) == QStringLiteral("$original"),
                 "replacement event id becomes interaction target");
    ok &= expect(komai::notificationStableId(notification) == QStringLiteral("$original"),
                 "stable id prefers replacement event id");
    return ok;
}

bool
testInviteStyleNotificationsFallBackToRoomId()
{
    komai::NotificationPayload notification{};
    notification.roomId = QStringLiteral("!invite:example.com");

    bool ok = true;
    ok &= expect(komai::notificationTargetEventId(notification).isEmpty(),
                 "target event id stays empty when no event id exists");
    ok &= expect(komai::notificationStableId(notification) == QStringLiteral("!invite:example.com"),
                 "stable id falls back to room id when no event id exists");
    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testDefaultsToEventIdWhenNoReplacementExists();
    ok &= testReplacementEventBecomesInteractionTarget();
    ok &= testInviteStyleNotificationsFallBackToRoomId();
    return ok ? 0 : 1;
}
