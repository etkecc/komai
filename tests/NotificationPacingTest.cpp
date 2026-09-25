// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>

#include "notifications/NotificationPacing.h"

namespace {

bool
expect(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

using komai::notificationPacing::PacingTracker;

bool
testDisabledPacingNeverSuppresses()
{
    PacingTracker tracker;
    const qint64 base = 10 * 3600 * 1000;
    tracker.recordDelivered(QStringLiteral("!a:example.com"), base);

    bool ok = true;
    ok &= expect(!tracker.isSuppressed(QStringLiteral("!a:example.com"), base + 60 * 1000, 0),
                 "pacing 0 never suppresses");
    ok &= expect(
      !tracker.isSuppressed(QStringLiteral("!a:example.com"), base + 5 * 60 * 1000, 0),
      "pacing 0 never suppresses even far after delivery");
    ok &= expect(
      !tracker.isSuppressed(QStringLiteral("!a:example.com"), base + 60 * 60 * 1000, -1),
      "negative pacing never suppresses");
    return ok;
}

bool
testNoEntryNeverSuppresses()
{
    PacingTracker tracker;
    return expect(!tracker.isSuppressed(QStringLiteral("!a:example.com"), 10 * 3600 * 1000, 5),
                  "first isSuppressed for an unknown room returns false");
}

bool
testUserScenarioWindow()
{
    PacingTracker tracker;
    const qint64 base = 10 * 3600 * 1000; // 10:00:00
    const QString room = QStringLiteral("!spam:example.com");

    bool ok = true;
    ok &= expect(!tracker.isSuppressed(room, base, 5), "10:00 first notification delivers");
    tracker.recordDelivered(room, base);
    ok &= expect(tracker.isSuppressed(room, base + 60 * 1000, 5), "10:01 suppressed");
    ok &= expect(tracker.isSuppressed(room, base + 90 * 1000, 5), "10:01:30 suppressed");
    ok &= expect(tracker.isSuppressed(room, base + 120 * 1000, 5), "10:02 suppressed");
    ok &= expect(tracker.isSuppressed(room, base + 180 * 1000, 5), "10:03 suppressed");
    ok &= expect(!tracker.isSuppressed(room, base + 5 * 60 * 1000, 5),
                 "10:05 boundary: elapsed == window is not suppressed");
    return ok;
}

bool
testDeliveryExtendsWindow()
{
    PacingTracker tracker;
    const qint64 base = 10 * 3600 * 1000;
    const QString room = QStringLiteral("!spam:example.com");
    tracker.recordDelivered(room, base + 5 * 60 * 1000);

    bool ok = true;
    ok &= expect(tracker.isSuppressed(room, base + 9 * 60 * 1000, 5),
                 "10:09 suppressed after 10:05 delivery");
    ok &= expect(!tracker.isSuppressed(room, base + 10 * 60 * 1000, 5),
                 "10:10 delivers after 10:05 delivery");
    return ok;
}

bool
testRoomsAreIndependent()
{
    PacingTracker tracker;
    const qint64 base = 10 * 3600 * 1000;
    tracker.recordDelivered(QStringLiteral("!a:example.com"), base);

    bool ok = true;
    ok &= expect(tracker.isSuppressed(QStringLiteral("!a:example.com"), base + 60 * 1000, 5),
                 "room A is suppressed");
    ok &= expect(!tracker.isSuppressed(QStringLiteral("!b:example.com"), base + 60 * 1000, 5),
                 "room B is not affected by room A delivery");
    return ok;
}

bool
testClearRemovesAllWindows()
{
    PacingTracker tracker;
    const qint64 base = 10 * 3600 * 1000;
    tracker.recordDelivered(QStringLiteral("!a:example.com"), base);
    tracker.recordDelivered(QStringLiteral("!b:example.com"), base);
    tracker.clear();

    bool ok = true;
    ok &= expect(!tracker.isSuppressed(QStringLiteral("!a:example.com"), base + 60 * 1000, 5),
                 "room A not suppressed after clear");
    ok &= expect(!tracker.isSuppressed(QStringLiteral("!b:example.com"), base + 60 * 1000, 5),
                 "room B not suppressed after clear");
    return ok;
}

bool
testClearRoomResetsOnlyThatRoom()
{
    PacingTracker tracker;
    const qint64 base = 10 * 3600 * 1000;
    const QString roomA = QStringLiteral("!a:example.com");
    const QString roomB = QStringLiteral("!b:example.com");
    tracker.recordDelivered(roomB, base);
    tracker.recordDelivered(roomA, base);
    tracker.clearRoom(roomA);

    bool ok = true;
    ok &= expect(!tracker.isSuppressed(roomA, base + 60 * 1000, 5),
                 "clearRoom resets room A window mid-window");
    ok &= expect(tracker.isSuppressed(roomB, base + 60 * 1000, 5),
                 "room B window untouched by clearRoom of room A");
    tracker.clearRoom(QStringLiteral("!never-seen:example.com"));
    ok &= expect(!tracker.isSuppressed(QStringLiteral("!never-seen:example.com"),
                                       base + 60 * 1000,
                                       5),
                 "clearRoom on a never-seen room is a no-op");
    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testDisabledPacingNeverSuppresses();
    ok &= testNoEntryNeverSuppresses();
    ok &= testUserScenarioWindow();
    ok &= testDeliveryExtendsWindow();
    ok &= testRoomsAreIndependent();
    ok &= testClearRemovesAllWindows();
    ok &= testClearRoomResetsOnlyThatRoom();
    return ok ? 0 : 1;
}
