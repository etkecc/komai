// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsPage.h"

int
UserSettings::communityListWidth() const
{
    return communityListWidth_;
}
int
UserSettings::roomListWidth() const
{
    return roomListWidth_;
}
double
UserSettings::scaleFactor() const
{
    return scaleFactor_ > 0.0 ? scaleFactor_ : 1.0;
}
double
UserSettings::fontSize() const
{
    if (const auto value = coreStore_.valueAs<double>(settings::core::SettingId::UiFontSizePt);
        value.has_value())
        return *value;
    return baseFontSize_;
}
QString
UserSettings::font() const
{
    if (const auto value = coreStore_.valueAs<std::string>(settings::core::SettingId::UiFontFamily);
        value.has_value())
        return QString::fromStdString(*value);
    return font_;
}
QString
UserSettings::emojiFontFamily() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::UiFontEmojiFamily);
        value.has_value())
        return QString::fromStdString(*value);
    return emojiFont_;
}
UserSettings::Presence
UserSettings::networkPresenceStatusPolicy() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NetworkPresenceStatusPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::Presence::AutomaticPresence) &&
        *value <= static_cast<int>(UserSettings::Presence::Offline))
        return static_cast<UserSettings::Presence>(*value);
    return networkPresenceStatusPolicy_;
}
UserSettings::ShowImage
UserSettings::timelineMediaImageDisplay() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMediaImageDisplay);
        value.has_value() && *value >= static_cast<int>(UserSettings::ShowImage::Always) &&
        *value <= static_cast<int>(UserSettings::ShowImage::Never))
        return static_cast<UserSettings::ShowImage>(*value);
    return timelineMediaImageDisplay_;
}
