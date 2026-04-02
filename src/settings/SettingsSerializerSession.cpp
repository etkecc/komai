// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include "komai-rust-cxxbridge/ffi.h"

#include <QString>

#include "logging/Logging.h"

#include "settings/ui/facade/UserSettingsPage.h"

namespace {

bool
hasSessionValue(const QString &value)
{
    return !value.trimmed().isEmpty();
}

} // namespace

namespace settings::serializer {

void
saveSession(const UserSettings &settings,
            const QString &sessionFilePath,
            ::komai::rust::SettingsProfileHandle &profileHandle)
{
    const bool hasUserId      = hasSessionValue(settings.userId());
    const bool hasDeviceId    = hasSessionValue(settings.deviceId());
    const bool hasHomeserver  = hasSessionValue(settings.homeserver());
    const bool hasAccessToken = hasSessionValue(settings.accessToken());

    if (!hasAccessToken)
        return;

    if (!hasUserId || !hasDeviceId || !hasHomeserver) {
        activeLoggers().ui->warn(
          "Skipping session.yml write because session identity is incomplete "
          "(has_user_id={}, has_device_id={}, has_homeserver={})",
          hasUserId,
          hasDeviceId,
          hasHomeserver);
        return;
    }

    ::komai::rust::settings_profile_replace_session_identity(profileHandle,
                                                             settings.userId().toStdString(),
                                                             settings.homeserver().toStdString(),
                                                             settings.deviceId().toStdString());
    const bool saved = ::komai::rust::settings_profile_write_session(profileHandle);
    if (saved) {
        activeLoggers().ui->debug("Saved session to: {}", sessionFilePath.toStdString());
    }
}

} // namespace settings::serializer
