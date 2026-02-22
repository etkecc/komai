// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>

#include <yaml-cpp/yaml.h>

#include "settings/StagedLoadPlan.h"
#include "settings/SettingKeys.h"

namespace settings::persistence {

struct SecretsPayload
{
    QString accessToken;
    QMap<QString, QString> secrets;
    bool hadStaleValues = false;
};

staged_load_plan::SecretsProvider
providerFromConfig(const YAML::Node &configRoot, bool runWithoutSecureSecretsService);

SecretsPayload loadProfileSecrets(const QString &profile,
                                 bool runWithoutSecureSecretsService,
                                 const QString &secretsFilePath);

void saveProfileSecrets(const QString &profile,
                       bool runWithoutSecureSecretsService,
                       const QString &secretsFilePath,
                       const QString &accessToken,
                       const QMap<QString, QString> &secrets);

bool clearProfileSecrets(const QString &profile, bool runWithoutSecureSecretsService, const QString &secretsFilePath);

} // namespace settings::persistence
