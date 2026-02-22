// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>

#include <yaml-cpp/yaml.h>
#include <memory>

#include "settings/SettingKeys.h"
#include "settings/StagedLoadPlan.h"

namespace spdlog {
class logger;
}

namespace settings::persistence {

struct PersistenceLoggers
{
    std::shared_ptr<spdlog::logger> ui;
};

void
setLoggers(PersistenceLoggers loggers);
PersistenceLoggers
activeLoggers();

/**
 * Functions that bridge settings persistence across file-backed and secure-backend
 * storage. This layer owns provider selection, validation/normalization of
 * profile secret payloads, and serialization/deserialization of secrets.
 */
struct SecretsPayload
{
    QString accessToken;
    QString sessionUserId;
    QString sessionDeviceId;
    QString sessionHomeserver;
    QMap<QString, QString> secrets;
    bool hadStaleValues = false;
};

/**
 * Resolve the effective secret provider taking command-line/runtime overrides into
 * account.
 */
staged_load_plan::SecretsProvider
providerFromConfig(const YAML::Node &configRoot, bool runWithoutSecureSecretsService);

/**
 * Load secrets/session auth payload for the given profile using the resolved
 * provider.
 */
SecretsPayload
loadProfileSecrets(const QString &profile,
                   bool runWithoutSecureSecretsService,
                   const QString &secretsFilePath);

/**
 * Persist session auth and profile secrets using the selected provider.
 */
void
saveProfileSecrets(const QString &profile,
                   bool runWithoutSecureSecretsService,
                   const QString &secretsFilePath,
                   const QString &accessToken,
                   const QMap<QString, QString> &secrets,
                   const QString &sessionUserId,
                   const QString &sessionDeviceId,
                   const QString &sessionHomeserver);

/**
 * Remove all persisted session secrets/auth for a profile from both file-backed and
 * secure backends.
 */
bool
clearProfileSecrets(const QString &profile,
                    bool runWithoutSecureSecretsService,
                    const QString &secretsFilePath);

} // namespace settings::persistence
