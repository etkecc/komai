// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsStorage.h"
#include "settings/ui/facade/UserSettingsPage.h"

void
UserSettings::loadConfigYaml(const YAML::Node &root)
{
    settings::serializer::loadConfig(*this, root);
}

void
UserSettings::loadSessionYaml(const YAML::Node &root)
{
    settings::serializer::loadSession(*this, root);
}

void
UserSettings::loadStateYaml(const YAML::Node &root)
{
    settings::serializer::loadState(*this, root);
}

void
UserSettings::saveConfigYaml() const
{
    settings::serializer::saveConfig(*this, configFilePath_);
}

void
UserSettings::saveSessionYaml() const
{
    settings::serializer::saveSession(*this, sessionFilePath_);
}

void
UserSettings::saveStateYaml() const
{
    settings::serializer::saveState(*this, stateFilePath_);
}

void
UserSettings::saveSecretsYaml() const
{
    const auto provider = settings::persistence::providerFromConfig(
      settings::storage::loadYamlFile(configFilePath_, "config"));
    settings::persistence::saveProfileSecrets(profile_,
                                              provider == staged_load_plan::SecretsProvider::File,
                                              secretsFilePath_,
                                              accessToken_,
                                              secrets_,
                                              userId_,
                                              deviceId_,
                                              homeserver_);
}
