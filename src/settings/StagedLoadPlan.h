// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>

#include <yaml-cpp/yaml.h>

#include "settings/YamlSettings.h"

namespace staged_load_plan {

enum class SecretsProvider
{
    SecretService,
    File,
};

enum class Stage
{
    Config,
    Session,
    SecretsSecureBackend,
    SecretsFile,
    State,
};

inline constexpr auto SecretsProviderKey = "secrets.provider";
inline constexpr auto ProviderSecretServiceValue = "secret_service";
inline constexpr auto ProviderFileValue          = "file";

inline SecretsProvider
providerFromConfig(const YAML::Node &configRoot)
{
    const auto provider = yaml_settings::readString(
      configRoot, SecretsProviderKey, QString::fromLatin1(ProviderSecretServiceValue));
    return provider == QLatin1String(ProviderFileValue) ? SecretsProvider::File
                                                         : SecretsProvider::SecretService;
}

inline QList<Stage>
stagesForProvider(SecretsProvider provider)
{
    return {
      Stage::Config,
      Stage::Session,
      provider == SecretsProvider::File ? Stage::SecretsFile : Stage::SecretsSecureBackend,
      Stage::State,
    };
}

} // namespace staged_load_plan

