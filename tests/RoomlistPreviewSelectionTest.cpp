// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

#include <QString>

#include "timeline/roomlist/RoomlistPreviewSelection.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

DescInfo
desc(QString eventId, QString body, QString descriptiveTime, quint64 timestamp)
{
    DescInfo info;
    info.event_id        = std::move(eventId);
    info.body            = std::move(body);
    info.descriptiveTime = std::move(descriptiveTime);
    info.timestamp       = timestamp;
    return info;
}

bool
testPrefersLiveMessageWhenResolved()
{
    const auto live = desc(QStringLiteral("$live"),
                           QStringLiteral("Live preview"),
                           QStringLiteral("Now"),
                           200);
    const auto cached = desc(QStringLiteral("$cached"),
                             QStringLiteral("Cached preview"),
                             QStringLiteral("Yesterday"),
                             150);

    const auto selected = timeline::roomlist::selectMaterializedPreviewFields(
      live, 200, true, cached, 180);

    bool ok = true;
    ok &= expect(selected.lastMessage == QStringLiteral("Live preview"),
                 "resolved live message is used");
    ok &= expect(selected.descriptiveTime == QStringLiteral("Now"),
                 "resolved live descriptive time is used");
    ok &= expect(selected.timestamp == 200, "timestamp keeps live timestamp when newer");
    return ok;
}

bool
testFallsBackToCachedWhenLiveNotResolved()
{
    const auto live = desc(QString(), QString(), QString(), 300);
    const auto cached = desc(QStringLiteral("$msg"),
                             QStringLiteral("Older real message"),
                             QStringLiteral("2d"),
                             240);

    const auto selected = timeline::roomlist::selectMaterializedPreviewFields(
      live, 300, false, cached, 260);

    bool ok = true;
    ok &= expect(selected.lastMessage == QStringLiteral("Older real message"),
                 "cached message is used when live preview is unresolved");
    ok &= expect(selected.descriptiveTime == QStringLiteral("2d"),
                 "cached descriptive time is used when live preview is unresolved");
    ok &= expect(selected.timestamp == 300,
                 "timestamp keeps latest activity timestamp even when text falls back");
    return ok;
}

bool
testFallsBackToCachedWhenLiveIsStateOnly()
{
    const auto live = desc(QStringLiteral("$join"),
                           QStringLiteral("You joined this room."),
                           QStringLiteral("1m"),
                           500);
    const auto cached = desc(QStringLiteral("$msg"),
                             QStringLiteral("Last actual message"),
                             QStringLiteral("1h"),
                             360);

    const auto selected = timeline::roomlist::selectMaterializedPreviewFields(
      live, 500, false, cached, 450);

    bool ok = true;
    ok &= expect(selected.lastMessage == QStringLiteral("Last actual message"),
                 "state-only live preview does not replace cached real message");
    ok &= expect(selected.descriptiveTime == QStringLiteral("1h"),
                 "state-only live preview does not replace cached time");
    ok &= expect(selected.timestamp == 500,
                 "timestamp still reflects most recent room activity");
    return ok;
}

bool
testUsesLiveFallbackWhenNoCachedPreview()
{
    const auto live = desc(QStringLiteral("$state"),
                           QStringLiteral("State update"),
                           QStringLiteral("3m"),
                           220);
    const std::optional<DescInfo> noCached = std::nullopt;

    const auto selected = timeline::roomlist::selectMaterializedPreviewFields(
      live, 220, false, noCached, 240);

    bool ok = true;
    ok &= expect(selected.lastMessage == QStringLiteral("State update"),
                 "live body is used when no cached preview exists");
    ok &= expect(selected.descriptiveTime == QStringLiteral("3m"),
                 "live descriptive time is used when no cached preview exists");
    ok &= expect(selected.timestamp == 240, "timestamp falls back to approximate room timestamp");
    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testPrefersLiveMessageWhenResolved();
    ok &= testFallsBackToCachedWhenLiveNotResolved();
    ok &= testFallsBackToCachedWhenLiveIsStateOnly();
    ok &= testUsesLiveFallbackWhenNoCachedPreview();

    return ok ? 0 : 1;
}
