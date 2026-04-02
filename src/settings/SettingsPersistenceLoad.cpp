// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include <QString>

#include "logging/Logging.h"

#include "profile/Paths.h"
#include "settings/SettingsStorage.h"

namespace settings::persistence {

SecretsPayload
loadProfileSecrets(const QString &profile, bool usesFileSecretsProvider)
{
    auto payload = detail::loadProfileSecretsPayload(profile, usesFileSecretsProvider);
    const auto normalizedProfile = app_paths::normalizedProfileId(profile);

    if (usesFileSecretsProvider) {
        activeLoggers().ui->info(
          "Loaded file-backed secrets (has_access_token={}, secrets_count={})",
          !payload.accessToken.trimmed().isEmpty(),
          payload.secrets.size());
        return payload;
    }

    if (payload.hadStaleValues) {
        activeLoggers().ui->warn("Found stale/empty secure backend values for profile '{}'",
                                 normalizedProfile.toStdString());
    }

    activeLoggers().ui->info(
      "Loaded secure-backend secrets (has_access_token={}, secrets_count={})",
      !payload.accessToken.trimmed().isEmpty(),
      payload.secrets.size());
    return payload;
}

} // namespace settings::persistence
