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
#include "ui/ThemeRegistry.h"
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

QColor
TimelineViewManager::roomUserColor(QString roomId,
                                   QString userId,
                                   QColor background,
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

    // Read user colors from the current theme.
    QColor selfColor;
    QList<QColor> othersColors;
    const auto settings = UserSettings::instance();
    if (settings) {
        const auto *def = ThemeRegistry::instance().findTheme(settings->uiThemeSlug());
        if (def) {
            selfColor = def->userColorSelf;
            othersColors.reserve(static_cast<qsizetype>(def->userColorOthers.size()));
            for (const auto &c : def->userColorOthers)
                othersColors.append(c);
        }
    }
    // Fallback if no theme found
    if (!selfColor.isValid())
        selfColor = QColor(0xf4, 0x93, 0x00); // Komai orange

    // Uniform "others" color = first color in the others list.
    const auto othersUniform = [&othersColors]() -> QColor {
        return othersColors.isEmpty() ? QColor::fromHsl(180, 80, 130) : othersColors.first();
    };

    if (isPreviewRoom) {
        if (policy == UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)
            return userId == selfId ? selfColor : othersUniform();

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
        return userId == selfId ? selfColor : othersUniform();

    auto memberCount   = static_cast<int>(cache::memberCount(roomId.toStdString()));
    int othersListSize = othersColors.size();

    // Dynamic threshold: if room has more members than the theme provides colors for,
    // use the uniform "others" color.
    if (othersListSize == 0 || memberCount > othersListSize) {
        return othersUniform();
    }

    // Small room: assign unique palette colors from the theme's others list.
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

    // Find this user's palette slot.
    auto it  = std::find(members.begin(), members.end(), userId.toStdString());
    int slot = 0;
    if (it != members.end())
        slot = static_cast<int>(std::distance(members.begin(), it));

    QColor color = othersColors[slot % othersListSize];

    roomUserColors_.insert(cacheKey, color);
    return color;
}
