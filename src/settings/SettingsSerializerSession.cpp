// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include "komai-rust-cxxbridge/lib.h"

#include <QString>

#include "logging/Logging.h"

#include "settings/SettingsStorage.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

bool
hasSessionValue(const QString &value)
{
    return !value.trimmed().isEmpty();
}

} // namespace

using settings::storage::writeTextFile;

namespace settings::serializer {

void
saveSession(const UserSettings &settings, const QString &sessionFilePath)
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

    const auto serialized =
      ::komai::rust::settings_encode_session_yaml(settings.userId().toStdString(),
                                                  settings.homeserver().toStdString(),
                                                  settings.deviceId().toStdString());

    if (writeTextFile(
          sessionFilePath, QString::fromStdString(static_cast<std::string>(serialized)), false)) {
        activeLoggers().ui->debug("Saved session to: {}", sessionFilePath.toStdString());
    }
}

} // namespace settings::serializer
