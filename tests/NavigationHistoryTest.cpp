// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QString>

#include "timeline/NavigationHistory.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testBackBasic()
{
    NavigationHistory h;
    h.push(QStringLiteral(""), QStringLiteral("room-a"));
    h.push(QStringLiteral(""), QStringLiteral("room-b"));

    auto entry = h.back(QStringLiteral(""), QStringLiteral("room-b"));

    bool ok = true;
    ok &= expect(entry.has_value(), "back returns an entry");
    ok &= expect(entry->roomId == QStringLiteral("room-a"), "back restores room-a");
    ok &= expect(entry->filterId == QStringLiteral(""), "back restores empty filter");
    return ok;
}

bool
testForwardBasic()
{
    NavigationHistory h;
    h.push(QStringLiteral(""), QStringLiteral("room-a"));
    h.push(QStringLiteral(""), QStringLiteral("room-b"));

    h.back(QStringLiteral(""), QStringLiteral("room-b"));
    auto entry = h.forward(QStringLiteral(""), QStringLiteral("room-a"));

    bool ok = true;
    ok &= expect(entry.has_value(), "forward returns an entry");
    ok &= expect(entry->roomId == QStringLiteral("room-b"), "forward restores room-b");
    return ok;
}

bool
testBackAtStartReturnsNullopt()
{
    NavigationHistory h;
    h.push(QStringLiteral(""), QStringLiteral("room-a"));

    auto entry = h.back(QStringLiteral(""), QStringLiteral("room-a"));

    bool ok = true;
    ok &= expect(!entry.has_value(), "back at start returns nullopt");
    return ok;
}

bool
testForwardAtEndReturnsNullopt()
{
    NavigationHistory h;
    h.push(QStringLiteral(""), QStringLiteral("room-a"));

    auto entry = h.forward(QStringLiteral(""), QStringLiteral("room-a"));

    bool ok = true;
    ok &= expect(!entry.has_value(), "forward at end returns nullopt");
    return ok;
}

bool
testDeduplication()
{
    NavigationHistory h;
    h.push(QStringLiteral(""), QStringLiteral("room-a"));
    h.push(QStringLiteral(""), QStringLiteral("room-a")); // duplicate

    bool ok = true;
    ok &= expect(!h.canGoBack(), "duplicate push does not create a new entry");
    return ok;
}

bool
testForwardTruncation()
{
    NavigationHistory h;
    h.push(QStringLiteral(""), QStringLiteral("room-a"));
    h.push(QStringLiteral(""), QStringLiteral("room-b"));
    h.push(QStringLiteral(""), QStringLiteral("room-c"));

    // Go back to room-a
    h.back(QStringLiteral(""), QStringLiteral("room-c"));
    h.back(QStringLiteral(""), QStringLiteral("room-b"));

    // New navigation from room-a should truncate room-b and room-c
    h.push(QStringLiteral(""), QStringLiteral("room-d"));

    bool ok = true;
    ok &= expect(!h.canGoForward(), "forward history is truncated after new push");

    auto entry = h.back(QStringLiteral(""), QStringLiteral("room-d"));
    ok &= expect(entry.has_value() && entry->roomId == QStringLiteral("room-a"),
                 "back after truncation goes to room-a");
    return ok;
}

bool
testFilterOnlyEntriesSkipped()
{
    NavigationHistory h;

    // Simulate: start with no room, open room-a, switch filter, open room-b
    h.push(QStringLiteral(""), QString());                                       // initial seed
    h.push(QStringLiteral(""), QStringLiteral("room-a"));                        // open room
    h.push(QStringLiteral("space:x"), QStringLiteral("room-a"), true);           // filter change
    h.push(QStringLiteral("space:x"), QStringLiteral("room-b"));                 // open room in space

    // Back should skip the filter-only entry and go to room-a with empty filter
    auto entry =
      h.back(QStringLiteral("space:x"), QStringLiteral("room-b"));

    bool ok = true;
    ok &= expect(entry.has_value(), "back returns an entry");
    ok &= expect(entry->roomId == QStringLiteral("room-a"),
                 "back skips filter-only entry and restores room-a");
    ok &= expect(entry->filterId == QStringLiteral(""),
                 "back restores the original empty filter");
    return ok;
}

bool
testFilterOnlyForwardSkipped()
{
    NavigationHistory h;

    h.push(QStringLiteral(""), QStringLiteral("room-a"));
    h.push(QStringLiteral("space:x"), QStringLiteral("room-a"), true);  // filter-only
    h.push(QStringLiteral("space:x"), QStringLiteral("room-b"));

    // Go all the way back
    h.back(QStringLiteral("space:x"), QStringLiteral("room-b"));
    // Now at room-a with empty filter. Forward should skip the filter-only entry.
    auto entry = h.forward(QStringLiteral(""), QStringLiteral("room-a"));

    bool ok = true;
    ok &= expect(entry.has_value(), "forward returns an entry");
    ok &= expect(entry->roomId == QStringLiteral("room-b"),
                 "forward skips filter-only entry and goes to room-b");
    ok &= expect(entry->filterId == QStringLiteral("space:x"),
                 "forward restores the space filter");
    return ok;
}

bool
testEmptyRoomEntryRestored()
{
    NavigationHistory h;

    h.push(QStringLiteral(""), QString());                // no room open
    h.push(QStringLiteral(""), QStringLiteral("room-a")); // open a room

    auto entry = h.back(QStringLiteral(""), QStringLiteral("room-a"));

    bool ok = true;
    ok &= expect(entry.has_value(), "back returns an entry");
    ok &= expect(entry->roomId.isEmpty(), "back restores empty room (no room open)");
    return ok;
}

bool
testMaxSizeCap()
{
    NavigationHistory h;

    // Push more than kMaxSize entries
    for (int i = 0; i < NavigationHistory::kMaxSize + 20; ++i) {
        h.push(QStringLiteral(""), QString::number(i));
    }

    // Should still be able to go back kMaxSize - 1 times
    int backCount = 0;
    QString lastRoom = QString::number(NavigationHistory::kMaxSize + 19);
    while (auto entry = h.back(QStringLiteral(""), lastRoom)) {
        lastRoom = entry->roomId;
        ++backCount;
    }

    bool ok = true;
    ok &= expect(backCount == NavigationHistory::kMaxSize - 1,
                 "back count equals kMaxSize - 1 after overflow");
    return ok;
}

bool
testMultipleBackForward()
{
    NavigationHistory h;
    h.push(QStringLiteral(""), QStringLiteral("room-a"));
    h.push(QStringLiteral(""), QStringLiteral("room-b"));
    h.push(QStringLiteral(""), QStringLiteral("room-c"));

    bool ok = true;

    auto e1 = h.back(QStringLiteral(""), QStringLiteral("room-c"));
    ok &= expect(e1.has_value() && e1->roomId == QStringLiteral("room-b"), "back to room-b");

    auto e2 = h.back(QStringLiteral(""), QStringLiteral("room-b"));
    ok &= expect(e2.has_value() && e2->roomId == QStringLiteral("room-a"), "back to room-a");

    auto e3 = h.back(QStringLiteral(""), QStringLiteral("room-a"));
    ok &= expect(!e3.has_value(), "back at start returns nullopt");

    auto e4 = h.forward(QStringLiteral(""), QStringLiteral("room-a"));
    ok &= expect(e4.has_value() && e4->roomId == QStringLiteral("room-b"), "forward to room-b");

    auto e5 = h.forward(QStringLiteral(""), QStringLiteral("room-b"));
    ok &= expect(e5.has_value() && e5->roomId == QStringLiteral("room-c"), "forward to room-c");

    auto e6 = h.forward(QStringLiteral(""), QStringLiteral("room-c"));
    ok &= expect(!e6.has_value(), "forward at end returns nullopt");

    return ok;
}

bool
testConsecutiveFilterOnlyEntriesAllSkipped()
{
    NavigationHistory h;

    h.push(QStringLiteral(""), QStringLiteral("room-a"));
    h.push(QStringLiteral("space:1"), QStringLiteral("room-a"), true);  // filter-only
    h.push(QStringLiteral("space:2"), QStringLiteral("room-a"), true);  // filter-only
    h.push(QStringLiteral("space:2"), QStringLiteral("room-b"));

    auto entry = h.back(QStringLiteral("space:2"), QStringLiteral("room-b"));

    bool ok = true;
    ok &= expect(entry.has_value(), "back returns an entry");
    ok &= expect(entry->roomId == QStringLiteral("room-a"),
                 "back skips all consecutive filter-only entries");
    ok &= expect(entry->filterId == QStringLiteral(""),
                 "back restores original filter");
    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testBackBasic();
    ok &= testForwardBasic();
    ok &= testBackAtStartReturnsNullopt();
    ok &= testForwardAtEndReturnsNullopt();
    ok &= testDeduplication();
    ok &= testForwardTruncation();
    ok &= testFilterOnlyEntriesSkipped();
    ok &= testFilterOnlyForwardSkipped();
    ok &= testEmptyRoomEntryRestored();
    ok &= testMaxSizeCap();
    ok &= testMultipleBackForward();
    ok &= testConsecutiveFilterOnlyEntriesAllSkipped();
    return ok ? 0 : 1;
}
