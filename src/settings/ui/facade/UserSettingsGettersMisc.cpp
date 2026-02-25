// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsPage.h"

QString
UserSettings::profile() const
{
    return profile_;
}
QString
UserSettings::userId() const
{
    return userId_;
}
QString
UserSettings::accessToken() const
{
    return accessToken_;
}
QString
UserSettings::deviceId() const
{
    return deviceId_;
}
QString
UserSettings::currentTagId() const
{
    return currentTagId_;
}
QString
UserSettings::homeserver() const
{
    return homeserver_;
}
bool
UserSettings::networkTlsEnableCertificateValidation() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::NetworkTlsEnableCertificateValidation);
        value.has_value())
        return *value;
    return networkTlsEnableCertificateValidation_;
}
QStringList
UserSettings::hiddenTags() const
{
    return hiddenTags_;
}
QStringList
UserSettings::mutedTags() const
{
    return mutedTags_;
}
QStringList
UserSettings::hiddenPins() const
{
    return hiddenPins_;
}
QStringList
UserSettings::hiddenWidgets() const
{
    return hiddenWidgets_;
}
QStringList
UserSettings::recentReactions() const
{
    return recentReactions_;
}
bool
UserSettings::timelineMediaOpenImagesExternal() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenImagesExternal);
        value.has_value())
        return *value;
    return timelineMediaOpenImagesExternal_;
}
bool
UserSettings::timelineMediaOpenVideosExternal() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenVideosExternal);
        value.has_value())
        return *value;
    return timelineMediaOpenVideosExternal_;
}
QList<QStringList>
UserSettings::collapsedSpaces() const
{
    return collapsedSpaces_;
}
int
UserSettings::integrationsDbusApiAccess() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::IntegrationsDbusApiAccess);
        value.has_value())
        return *value;
    return integrationsDbusApiAccess_;
}
QString
UserSettings::integrationsLinksBrowserCommand() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::IntegrationsBrowserCommand);
        value.has_value())
        return QString::fromStdString(*value);
    return integrationsLinksBrowserCommand_;
}
bool
UserSettings::privacyMaintenanceUpdateSpaceVias() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias);
        value.has_value())
        return *value;
    return privacyMaintenanceUpdateSpaceVias_;
}
bool
UserSettings::privacyMaintenanceExpireEvents() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyMaintenanceExpireEvents);
        value.has_value())
        return *value;
    return privacyMaintenanceExpireEvents_;
}
int
UserSettings::windowWidth() const
{
    return windowWidth_;
}
int
UserSettings::windowHeight() const
{
    return windowHeight_;
}
qulonglong
UserSettings::maxDbSize() const
{
    return maxDbSize_;
}
uint
UserSettings::maxStores() const
{
    return maxStores_;
}
// Internal helper: secrets provider is configured via `secrets.provider` in config.yml.
bool
UserSettings::usesFileSecretsProvider() const
{
    return usesFileSecretsProvider_;
}
bool
UserSettings::networkHttp3Enabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::NetworkHttp3Enabled);
        value.has_value())
        return *value;
    return networkHttp3Enabled_;
}
settings::core::SettingsStore &
UserSettings::mutableCoreStore()
{
    return coreStore_;
}
const settings::core::SettingsStore &
UserSettings::coreStore() const
{
    return coreStore_;
}
