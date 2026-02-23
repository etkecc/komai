// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsPage.h"

QString
UserSettings::ringtone() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsAudioRingtone);
        value.has_value())
        return QString::fromStdString(*value);
    return ringtone_;
}
QString
UserSettings::microphone() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesMicrophone);
        value.has_value())
        return QString::fromStdString(*value);
    return microphone_;
}
QString
UserSettings::camera() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesCamera);
        value.has_value())
        return QString::fromStdString(*value);
    return camera_;
}
QString
UserSettings::cameraResolution() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesCameraResolution);
        value.has_value())
        return QString::fromStdString(*value);
    return cameraResolution_;
}
QString
UserSettings::cameraFrameRate() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesCameraFrameRate);
        value.has_value())
        return QString::fromStdString(*value);
    return cameraFrameRate_;
}
int
UserSettings::screenShareFrameRate() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::CallsScreenshareFrameRate);
        value.has_value())
        return *value;
    return screenShareFrameRate_;
}
bool
UserSettings::screenSharePiP() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::CallsScreensharePictureInPicture);
        value.has_value())
        return *value;
    return screenSharePiP_;
}
bool
UserSettings::screenShareRemoteVideo() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::CallsScreenshareIncludeRemoteVideo);
        value.has_value())
        return *value;
    return screenShareRemoteVideo_;
}
bool
UserSettings::screenShareShowCursor() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::CallsScreenshareShowCursor);
        value.has_value())
        return *value;
    return screenShareShowCursor_;
}
bool
UserSettings::fallbackCallRelayServerEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::CallsRelayUseFallbackServer);
        value.has_value())
        return *value;
    return fallbackCallRelayServerEnabled_;
}
bool
UserSettings::legacyCallsEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::CallsLegacyEnabled);
        value.has_value())
        return *value;
    return legacyCallsEnabled_;
}
bool
UserSettings::shareKeysWithTrustedUsers() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::EncryptionKeySharingShareWithTrusted);
        value.has_value())
        return *value;
    return shareKeysWithTrustedUsers_;
}
bool
UserSettings::onlyShareKeysWithVerifiedUsers() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers);
        value.has_value())
        return *value;
    return onlyShareKeysWithVerifiedUsers_;
}
bool
UserSettings::onlineKeyBackupEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::EncryptionBackupOnlineEnabled);
        value.has_value())
        return *value;
    return onlineKeyBackupEnabled_;
}
