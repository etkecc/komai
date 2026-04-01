// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>

#include <memory>
#include <yaml-cpp/yaml.h>

#include "settings/SettingKeys.h"
#include "settings/StagedLoadPlan.h"

namespace nhlog {
class Logger;
}

namespace settings::persistence {

struct PersistenceLoggers
{
    std::shared_ptr<nhlog::Logger> ui;
};

void
setLoggers(PersistenceLoggers loggers);
const PersistenceLoggers &
activeLoggers();

/**
 * Functions that bridge settings persistence across file-backed and secure-backend
 * storage. This layer owns provider selection, validation/normalization of
 * profile secret payloads, and serialization/deserialization of secrets.
 *
 * Access token is stored in the secrets payload under internal
 * `__session.access_token` for both providers.
 */
struct SecretsPayload
{
    QString accessToken;
    QMap<QString, QString> secrets;
    bool hadStaleValues = false;
};

/**
 * Resolve the effective secret provider from `config.yml`.
 */
staged_load_plan::SecretsProvider
providerFromConfig(const YAML::Node &configRoot);

/**
 * Load secrets/session auth payload for the given profile using the resolved
 * provider.
 */
SecretsPayload
loadProfileSecrets(const QString &profile,
                   bool usesFileSecretsProvider,
                   const QString &secretsFilePath);

/**
 * Persist session auth and profile secrets using the selected provider.
 */
void
saveProfileSecrets(const QString &profile,
                   bool usesFileSecretsProvider,
                   const QString &secretsFilePath,
                   const QString &accessToken,
                   const QMap<QString, QString> &secrets);

/**
 * Remove all persisted session secrets/auth for a profile from both file-backed and
 * secure backends.
 */
bool
clearProfileSecrets(const QString &profile,
                    bool usesFileSecretsProvider,
                    const QString &secretsFilePath);

} // namespace settings::persistence
