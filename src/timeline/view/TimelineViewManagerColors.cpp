// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <QGuiApplication>
#include <QSet>
#include <QVariantMap>

#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/Theme.h"
#include "ui/ThemeRegistry.h"
#include "utils/Utils.h"

namespace {

QVariantMap
bubblePaletteMap(const QPalette &palette)
{
    QVariantMap map;
    const auto color = [&palette](QPalette::ColorRole role) {
        return palette.color(QPalette::Active, role);
    };

    map.insert(QStringLiteral("window"), color(QPalette::Window));
    map.insert(QStringLiteral("windowText"), color(QPalette::WindowText));
    map.insert(QStringLiteral("base"), color(QPalette::Base));
    map.insert(QStringLiteral("alternateBase"), color(QPalette::AlternateBase));
    map.insert(QStringLiteral("text"), color(QPalette::Text));
    map.insert(QStringLiteral("brightText"), color(QPalette::BrightText));
    map.insert(QStringLiteral("button"), color(QPalette::Button));
    map.insert(QStringLiteral("buttonText"), color(QPalette::ButtonText));
    map.insert(QStringLiteral("light"), color(QPalette::Light));
    map.insert(QStringLiteral("mid"), color(QPalette::Mid));
    map.insert(QStringLiteral("dark"), color(QPalette::Dark));
    map.insert(QStringLiteral("highlight"), color(QPalette::Highlight));
    map.insert(QStringLiteral("highlightedText"), color(QPalette::HighlightedText));
    map.insert(QStringLiteral("link"), color(QPalette::Link));
    map.insert(QStringLiteral("toolTipBase"), color(QPalette::ToolTipBase));
    map.insert(QStringLiteral("toolTipText"), color(QPalette::ToolTipText));
    return map;
}

QColor
blendColor(const QColor &background, const QColor &foreground, qreal alpha)
{
    return QColor::fromRgbF(background.redF() * (1.0 - alpha) + foreground.redF() * alpha,
                            background.greenF() * (1.0 - alpha) + foreground.greenF() * alpha,
                            background.blueF() * (1.0 - alpha) + foreground.blueF() * alpha,
                            1.0);
}

ThemeUserColorSlot
resolveBubbleSlot(const ThemeUserColorSlot &slot,
                  const QPalette &themePalette,
                  const QColor &background)
{
    ThemeUserColorSlot resolved = slot;

    if (!resolved.background.isValid())
        resolved.background = background;
    if (!resolved.text.isValid())
        resolved.text = themePalette.color(QPalette::Active, QPalette::Text);
    if (!resolved.secondaryText.isValid())
        resolved.secondaryText = themePalette.color(QPalette::Active, QPalette::ButtonText);
    if (!resolved.link.isValid())
        resolved.link = themePalette.color(QPalette::Active, QPalette::Link);

    return resolved;
}

QPalette
buildBubblePalette(const ThemeUserColorSlot &slot,
                   const QPalette &themePalette,
                   const QColor &background)
{
    const auto resolved = resolveBubbleSlot(slot, themePalette, background);

    const QColor alternateSurface =
      blendColor(resolved.background, themePalette.color(QPalette::AlternateBase), 0.35);
    const QColor hoverSurface =
      blendColor(resolved.background, themePalette.color(QPalette::Dark), 0.55);
    const QColor midSurface =
      blendColor(resolved.background, themePalette.color(QPalette::Mid), 0.5);
    const QColor lightSurface =
      blendColor(resolved.background, themePalette.color(QPalette::Light), 0.25);

    QPalette bubblePalette = themePalette;
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        bubblePalette.setColor(group, QPalette::Window, resolved.background);
        bubblePalette.setColor(group, QPalette::Base, resolved.background);
        bubblePalette.setColor(group, QPalette::Button, resolved.background);
        bubblePalette.setColor(group, QPalette::AlternateBase, alternateSurface);
        bubblePalette.setColor(group, QPalette::Dark, hoverSurface);
        bubblePalette.setColor(group, QPalette::Mid, midSurface);
        bubblePalette.setColor(group, QPalette::Light, lightSurface);
        bubblePalette.setColor(group,
                               QPalette::Text,
                               group == QPalette::Inactive ? resolved.secondaryText
                                                           : resolved.text);
        bubblePalette.setColor(group,
                               QPalette::WindowText,
                               group == QPalette::Inactive ? resolved.secondaryText
                                                           : resolved.text);
        bubblePalette.setColor(group, QPalette::BrightText, resolved.text);
        bubblePalette.setColor(group, QPalette::ButtonText, resolved.secondaryText);
        bubblePalette.setColor(group, QPalette::Link, resolved.link);
        bubblePalette.setColor(group, QPalette::ToolTipBase, alternateSurface);
        bubblePalette.setColor(group, QPalette::ToolTipText, resolved.text);
    }

    return bubblePalette;
}

int
assignPreviewBubbleSlot(QHash<std::pair<QString, QString>, int> &slotCache,
                        const QString &roomId,
                        const QString &userId,
                        int slotCount)
{
    if (slotCount <= 0)
        return 0;

    const std::pair<QString, QString> cacheKey{roomId, userId};
    if (slotCache.contains(cacheKey))
        return slotCache.value(cacheKey);

    QSet<int> usedSlots;
    for (auto it = slotCache.cbegin(); it != slotCache.cend(); ++it) {
        if (it.key().first == roomId)
            usedSlots.insert(it.value() % slotCount);
    }

    const int start = static_cast<int>(qHash(QStringLiteral("%1|%2").arg(roomId, userId)) %
                                       static_cast<uint>(slotCount));
    for (int offset = 0; offset < slotCount; ++offset) {
        const int candidate = (start + offset) % slotCount;
        if (!usedSlots.contains(candidate)) {
            slotCache.insert(cacheKey, candidate);
            return candidate;
        }
    }

    slotCache.insert(cacheKey, start);
    return start;
}

ThemeUserColorSlot
previewThemeSlot(const ThemeDef *def,
                 const QString &roomId,
                 const QString &userId,
                 const QString &selfId,
                 int roomMemberCount,
                 bool respectRoomSize,
                 UserSettings::TimelineUserColorCodingPolicy policy,
                 QHash<std::pair<QString, QString>, int> &slotCache,
                 QColor fallbackBackground)
{
    ThemeUserColorSlot fallbackSlot;
    fallbackSlot.background = fallbackBackground;

    if (!def)
        return fallbackSlot;

    const auto selfSlot = [&def, fallbackBackground]() {
        auto slot = def->userColorSelf;
        if (!slot.background.isValid())
            slot.background =
              fallbackBackground.isValid() ? fallbackBackground : QColor(0xf4, 0x93, 0x00);
        return slot;
    };

    const auto otherSlotAt = [&def, fallbackBackground](int index) {
        if (def->userColorOthers.empty()) {
            ThemeUserColorSlot slot;
            slot.background = fallbackBackground;
            return slot;
        }

        auto slot = def->userColorOthers[static_cast<size_t>(
          index % static_cast<int>(def->userColorOthers.size()))];
        if (!slot.background.isValid())
            slot.background = fallbackBackground.isValid()
                                ? fallbackBackground
                                : def->userColorOthers.front().background;
        return slot;
    };

    if (policy == UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)
        return userId == selfId ? selfSlot() : otherSlotAt(0);

    if (userId == selfId)
        return selfSlot();

    const int othersListSize = static_cast<int>(def->userColorOthers.size());
    if (othersListSize <= 0)
        return fallbackSlot;

    if (respectRoomSize) {
        const int otherMemberCount = std::max(0, roomMemberCount - 1);
        if (otherMemberCount > othersListSize)
            return otherSlotAt(0);
    }

    const int slotIndex = assignPreviewBubbleSlot(slotCache, roomId, userId, othersListSize);
    return otherSlotAt(slotIndex);
}

UserSettings::TimelineUserColorCodingPolicy
resolveColorCodingPolicy(int colorCodingPolicy)
{
    if (colorCodingPolicy >=
          static_cast<int>(UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize) &&
        colorCodingPolicy <=
          static_cast<int>(UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)) {
        return static_cast<UserSettings::TimelineUserColorCodingPolicy>(colorCodingPolicy);
    }

    const auto settings = UserSettings::instance();
    return settings ? settings->timelineUserColorCodingPolicy()
                    : UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize;
}

QPalette
currentThemePalette()
{
    const auto settings = UserSettings::instance();
    return settings ? Theme::paletteFromTheme(settings->uiThemeSlug()) : QGuiApplication::palette();
}

const ThemeDef *
currentThemeDef()
{
    const auto settings = UserSettings::instance();
    return settings ? ThemeRegistry::instance().findTheme(settings->uiThemeSlug()) : nullptr;
}

QColor
formerMemberColor(const QColor &background)
{
    auto bgLightness = background.lightnessF();
    if (bgLightness > 0.5)
        return QColor::fromHsl(0, 0, 180); // light theme: medium-light gray
    else
        return QColor::fromHsl(0, 0, 100); // dark theme: medium-dark gray
}

} // namespace

void
TimelineViewManager::updateColorPalette()
{
    userColors.clear();
    roomUserColors_.clear();
    roomUserColorSlots_.clear();
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

QVariantMap
TimelineViewManager::userBubblePalette(QString id, QColor background)
{
    auto themePalette    = currentThemePalette();
    const auto *themeDef = currentThemeDef();

    if (themeDef && id == utils::localUser()) {
        const auto palette = buildBubblePalette(
          themeDef->userColorSelf, themePalette, themeDef->userColorSelf.background);
        return bubblePaletteMap(palette);
    }

    if (themeDef && !themeDef->userColorOthers.empty()) {
        const auto &slot   = themeDef->userColorOthers.front();
        const auto palette = buildBubblePalette(slot, themePalette, slot.background);
        return bubblePaletteMap(palette);
    }

    ThemeUserColorSlot slot;
    slot.background    = userColor(id, background);
    const auto palette = buildBubblePalette(slot, themePalette, slot.background);
    return bubblePaletteMap(palette);
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

    const auto policy = resolveColorCodingPolicy(colorCodingPolicy);

    // Read user colors from the current theme.
    QColor selfColor;
    QList<QColor> othersColors;
    if (const auto *def = currentThemeDef()) {
        selfColor = def->userColorSelf.background;
        othersColors.reserve(static_cast<qsizetype>(def->userColorOthers.size()));
        for (const auto &slot : def->userColorOthers)
            othersColors.append(slot.background);
    }
    // Fallback if no theme found
    if (!selfColor.isValid())
        selfColor = QColor(0xf4, 0x93, 0x00); // Komai orange

    // Uniform "others" color = first color in the others list.
    const auto othersUniform = [&othersColors]() -> QColor {
        return othersColors.isEmpty() ? QColor::fromHsl(180, 80, 130) : othersColors.first();
    };

    if (isPreviewRoom) {
        const auto slot = previewThemeSlot(currentThemeDef(),
                                           roomId,
                                           userId,
                                           selfId,
                                           -1,
                                           false,
                                           policy,
                                           roomUserColorSlots_,
                                           background);
        return resolveBubbleSlot(slot, currentThemePalette(), background).background;
    }

    // Former member: return a neutral gray regardless of room size.
    if (!cache::isRoomMember(userId.toStdString(), roomId.toStdString()))
        return formerMemberColor(background);

    if (userId == selfId)
        return selfColor;

    if (policy == UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)
        return othersUniform();

    auto memberCount           = static_cast<int>(cache::memberCount(roomId.toStdString()));
    const int otherMemberCount = std::max(0, memberCount - 1);
    int othersListSize         = othersColors.size();

    // Dynamic threshold: if room has more members than the theme provides colors for,
    // use the uniform "others" color.
    if (othersListSize == 0 || otherMemberCount > othersListSize) {
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
        auto slotIt = roomUserColorSlots_.begin();
        while (slotIt != roomUserColorSlots_.end()) {
            if (slotIt.key().first == roomId)
                slotIt = roomUserColorSlots_.erase(slotIt);
            else
                ++slotIt;
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
    roomUserColorSlots_.insert(cacheKey, slot);
    return color;
}

QVariantMap
TimelineViewManager::roomUserBubblePalette(QString roomId,
                                           QString userId,
                                           QColor background,
                                           int colorCodingPolicy)
{
    const auto themePalette  = currentThemePalette();
    const auto *def          = currentThemeDef();
    const auto policy        = resolveColorCodingPolicy(colorCodingPolicy);
    const auto selfId        = utils::localUser();
    const bool isPreviewRoom = roomId.startsWith(QLatin1String("!timeline-preview:"));

    if (!def)
        return userBubblePalette(userId, background);

    if (roomId.isEmpty() || userId.isEmpty()) {
        const auto palette =
          buildBubblePalette(def->userColorSelf, themePalette, def->userColorSelf.background);
        return bubblePaletteMap(palette);
    }

    if (policy == UserSettings::TimelineUserColorCodingPolicy::MeVsOthers) {
        if (userId == selfId) {
            const auto palette =
              buildBubblePalette(def->userColorSelf, themePalette, def->userColorSelf.background);
            return bubblePaletteMap(palette);
        }

        const auto slot =
          def->userColorOthers.empty() ? ThemeUserColorSlot{} : def->userColorOthers.front();
        const auto palette =
          buildBubblePalette(slot,
                             themePalette,
                             def->userColorOthers.empty()
                               ? roomUserColor(roomId, userId, background, colorCodingPolicy)
                               : def->userColorOthers.front().background);
        return bubblePaletteMap(palette);
    }

    if (isPreviewRoom) {
        const auto slot = previewThemeSlot(
          def, roomId, userId, selfId, -1, false, policy, roomUserColorSlots_, background);
        const auto palette = buildBubblePalette(slot, themePalette, background);
        return bubblePaletteMap(palette);
    }

    if (!cache::isRoomMember(userId.toStdString(), roomId.toStdString())) {
        ThemeUserColorSlot slot;
        slot.background    = formerMemberColor(background);
        const auto palette = buildBubblePalette(slot, themePalette, slot.background);
        return bubblePaletteMap(palette);
    }

    if (userId == selfId) {
        const auto palette =
          buildBubblePalette(def->userColorSelf, themePalette, def->userColorSelf.background);
        return bubblePaletteMap(palette);
    }

    if (def->userColorOthers.empty()) {
        ThemeUserColorSlot slot;
        slot.background    = roomUserColor(roomId, userId, background, colorCodingPolicy);
        const auto palette = buildBubblePalette(slot, themePalette, slot.background);
        return bubblePaletteMap(palette);
    }

    auto memberCount           = static_cast<int>(cache::memberCount(roomId.toStdString()));
    const int otherMemberCount = std::max(0, memberCount - 1);
    const int othersListSize   = static_cast<int>(def->userColorOthers.size());
    if (otherMemberCount > othersListSize) {
        const auto palette = buildBubblePalette(
          def->userColorOthers.front(), themePalette, def->userColorOthers.front().background);
        return bubblePaletteMap(palette);
    }

    std::pair<QString, QString> cacheKey{roomId, userId};
    if (!roomUserColorSlots_.contains(cacheKey))
        (void)roomUserColor(roomId, userId, background, colorCodingPolicy);

    const int slotIndex = roomUserColorSlots_.value(cacheKey, 0);
    const auto &slot    = def->userColorOthers[slotIndex % othersListSize];
    const auto palette  = buildBubblePalette(slot, themePalette, slot.background);
    return bubblePaletteMap(palette);
}

QColor
TimelineViewManager::previewRoomUserColor(QString roomId,
                                          QString userId,
                                          QColor background,
                                          int roomMemberCount,
                                          int colorCodingPolicy)
{
    if (roomId.isEmpty() || userId.isEmpty())
        return QColor();

    const auto themePalette = currentThemePalette();
    const auto slot         = previewThemeSlot(currentThemeDef(),
                                       roomId,
                                       userId,
                                       utils::localUser(),
                                       roomMemberCount,
                                       true,
                                       resolveColorCodingPolicy(colorCodingPolicy),
                                       roomUserColorSlots_,
                                       background);
    return resolveBubbleSlot(slot, themePalette, background).background;
}

QVariantMap
TimelineViewManager::previewRoomUserBubblePalette(QString roomId,
                                                  QString userId,
                                                  QColor background,
                                                  int roomMemberCount,
                                                  int colorCodingPolicy)
{
    const auto themePalette = currentThemePalette();
    const auto *def         = currentThemeDef();
    const auto policy       = resolveColorCodingPolicy(colorCodingPolicy);

    if (!def)
        return userBubblePalette(userId, background);

    const auto slot    = previewThemeSlot(def,
                                       roomId,
                                       userId,
                                       utils::localUser(),
                                       roomMemberCount,
                                       true,
                                       policy,
                                       roomUserColorSlots_,
                                       background);
    const auto palette = buildBubblePalette(slot, themePalette, background);
    return bubblePaletteMap(palette);
}
