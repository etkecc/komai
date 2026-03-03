// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

void
TimelineViewManager::updateColorPalette()
{
    userColors.clear();
    roomUserColors_.clear();
    roomMemberCache_.clear();
}

QColor
TimelineViewManager::userColor(QString id, QColor background)
{
    std::pair<QString, quint64> idx{id, background.rgba64()};
    if (!userColors.contains(idx))
        userColors.insert(idx, QColor(utils::generateContrastingHexColor(id, background)));
    return userColors.value(idx);
}

// 16 maximally-spaced hues (in degrees) for small-room palette assignment.
// Ordered so that adjacent slots have large hue separation (golden-angle inspired).
const std::vector<double> TimelineViewManager::kPaletteHues = {
  0,     // red
  137.5, // green-cyan
  275,   // violet
  52.5,  // amber/yellow
  190,   // cyan-blue
  327.5, // magenta-pink
  95,    // lime/chartreuse
  232.5, // blue-indigo
  22.5,  // orange-red
  160,   // teal
  297.5, // purple
  75,    // yellow-green
  212.5, // azure
  350,   // rose
  117.5, // green
  255,   // blue-violet
};

QColor
TimelineViewManager::roomUserColor(QString roomId,
                                   QString userId,
                                   QColor background,
                                   QColor accentColor,
                                   int colorCodingPolicy)
{
    // Guard against empty strings (e.g. event data not yet loaded) to avoid
    // backend key-size errors from cache lookups with zero-length keys.
    if (roomId.isEmpty() || userId.isEmpty())
        return QColor();

    auto selfId              = utils::localUser();
    const bool isPreviewRoom = roomId.startsWith(QLatin1String("!timeline-preview:"));

    const auto policy = [colorCodingPolicy]() {
        if (colorCodingPolicy >=
              static_cast<int>(UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize) &&
            colorCodingPolicy <=
              static_cast<int>(UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)) {
            return static_cast<UserSettings::TimelineUserColorCodingPolicy>(colorCodingPolicy);
        }

        const auto settings = UserSettings::instance();
        return settings ? settings->timelineUserColorCodingPolicy()
                        : UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize;
    }();

    const auto othersColor = [accentColor]() {
        // Hue is offset 150 degrees from the theme accent so it always contrasts
        // with the sender's own bubble color (e.g. orange accent -> teal, blue -> magenta).
        double accentHue = accentColor.hslHue();
        int neutralHue   = (static_cast<int>(accentHue + 150)) % 360;
        return QColor::fromHsl(neutralHue, 80, 130);
    };

    if (isPreviewRoom) {
        if (policy == UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)
            return userId == selfId ? accentColor : othersColor();

        // Settings preview uses a synthetic room that does not exist in cache; generate stable
        // per-member colors directly from ids so color-coding policy changes remain visible.
        return userColor(QStringLiteral("%1|%2").arg(roomId, userId), background);
    }

    // Former member: return a neutral gray regardless of room size.
    if (!cache::isRoomMember(userId.toStdString(), roomId.toStdString())) {
        auto bgLightness = background.lightnessF();
        if (bgLightness > 0.5)
            return QColor::fromHsl(0, 0, 180); // light theme: medium-light gray
        else
            return QColor::fromHsl(0, 0, 100); // dark theme: medium-dark gray
    }

    if (policy == UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)
        return userId == selfId ? accentColor : othersColor();

    auto memberCount = static_cast<int>(cache::memberCount(roomId.toStdString()));

    // Large room (>16 members): return a uniform accent-complementary color.
    if (memberCount > 16) {
        return othersColor();
    }

    // Small room (<=16 members): assign unique palette colors.
    std::pair<QString, QString> cacheKey{roomId, userId};
    if (roomUserColors_.contains(cacheKey))
        return roomUserColors_.value(cacheKey);

    // Build or retrieve the sorted member list (excluding self).
    // Invalidate if cached size doesn't match current member count.
    bool needsRefresh = !roomMemberCache_.contains(roomId);
    if (!needsRefresh) {
        auto cachedSize = static_cast<int>(roomMemberCache_.value(roomId).size());
        if (cachedSize + 1 != memberCount)
            needsRefresh = true;
    }
    if (needsRefresh) {
        auto it = roomUserColors_.begin();
        while (it != roomUserColors_.end()) {
            if (it.key().first == roomId)
                it = roomUserColors_.erase(it);
            else
                ++it;
        }

        auto members = cache::roomMembers(roomId.toStdString());
        members.erase(std::remove(members.begin(), members.end(), selfId.toStdString()),
                      members.end());
        std::sort(members.begin(), members.end());
        roomMemberCache_.insert(roomId, members);
    }

    const auto &members = roomMemberCache_.value(roomId);

    // Filter palette hues that are too close to the accent color (self bubble hue).
    double accentHue                = accentColor.hslHueF() * 360.0;
    constexpr double kExclusionZone = 30.0; // degrees on each side

    std::vector<double> filteredHues;
    filteredHues.reserve(kPaletteHues.size());
    for (double h : kPaletteHues) {
        double diff = std::abs(h - accentHue);
        if (diff > 180.0)
            diff = 360.0 - diff;
        if (diff >= kExclusionZone)
            filteredHues.push_back(h);
    }
    // Fallback: if too many hues were filtered, use the full palette.
    if (filteredHues.size() < 8)
        filteredHues = kPaletteHues;

    // Find this user's palette slot.
    auto it  = std::find(members.begin(), members.end(), userId.toStdString());
    int slot = 0;
    if (it != members.end())
        slot = static_cast<int>(std::distance(members.begin(), it));

    double hue   = filteredHues[static_cast<size_t>(slot) % filteredHues.size()];
    QColor color = QColor::fromHslF(hue / 360.0, 0.7, 0.5);

    roomUserColors_.insert(cacheKey, color);
    return color;
}
