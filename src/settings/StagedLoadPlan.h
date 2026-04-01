// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringView>

namespace staged_load_plan {

/**
 * Secret backend selection and staged loading order used during startup.
 */
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

inline constexpr auto SecretsProviderKey         = "secrets.provider";
inline constexpr auto ProviderSecretServiceValue = "secret_service";
inline constexpr auto ProviderFileValue          = "file";

/**
 * Resolve the effective secret provider from a config token value.
 */
inline SecretsProvider
providerFromConfigValue(QStringView provider)
{
    return provider == QLatin1String(ProviderFileValue) ? SecretsProvider::File
                                                        : SecretsProvider::SecretService;
}

/**
 * Return the ordered list of stages to apply after reading config.
 */
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
