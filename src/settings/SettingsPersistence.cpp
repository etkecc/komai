// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "settings/SettingKeys.h"

#include <string>
#include <string_view>

namespace settings::persistence {

namespace {

PersistenceLoggers
defaultLoggers()
{
    return {.ui = std::make_shared<nhlog::Logger>("settings-persistence-ui")};
}

PersistenceLoggers &
currentLoggers()
{
    static PersistenceLoggers loggers = defaultLoggers();
    return loggers;
}

} // namespace

void
setLoggers(PersistenceLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    currentLoggers() = std::move(loggers);
}

const PersistenceLoggers &
activeLoggers()
{
    return currentLoggers();
}

staged_load_plan::SecretsProvider
providerFromConfigValue(QStringView providerValue)
{
    return staged_load_plan::providerFromConfigValue(providerValue);
}

namespace detail {

namespace {

::rust::Vec<::komai::rust::SettingsStringMapEntry>
toRustStringMapEntries(const QMap<QString, QString> &secrets)
{
    ::rust::Vec<::komai::rust::SettingsStringMapEntry> entries;
    for (auto it = secrets.constBegin(); it != secrets.constEnd(); ++it) {
        entries.push_back({
          .key   = it.key().toStdString(),
          .value = it.value().toStdString(),
        });
    }
    return entries;
}

QMap<QString, QString>
fromRustStringMapEntries(const ::rust::Vec<::komai::rust::SettingsStringMapEntry> &entries)
{
    QMap<QString, QString> secrets;
    for (const auto &entry : entries) {
        secrets[QString::fromStdString(static_cast<std::string>(entry.key))] =
          QString::fromStdString(static_cast<std::string>(entry.value));
    }
    return secrets;
}

SecretsPayload
fromRustSecretsPayload(const ::komai::rust::SettingsSecretsPayload &payload)
{
    return {
      .accessToken    = QString::fromStdString(static_cast<std::string>(payload.access_token)),
      .secrets        = fromRustStringMapEntries(payload.secrets),
      .hadStaleValues = payload.had_stale_values,
    };
}

} // namespace

QString
encodePersistedSecretsMap(const QString &accessToken, const QMap<QString, QString> &secrets)
{
    const auto encoded = ::komai::rust::settings_encode_persisted_secrets_map_yaml(
      accessToken.toStdString(), toRustStringMapEntries(secrets));
    return QString::fromStdString(static_cast<std::string>(encoded));
}

SecretsPayload
decodePersistedSecretsMap(const QString &serialized)
{
    return fromRustSecretsPayload(
      ::komai::rust::settings_decode_persisted_secrets_map_yaml(serialized.toStdString()));
}

SecretsPayload
loadPersistedSecretsFilePayloadFromPath(const QString &path, const char *label)
{
    return fromRustSecretsPayload(::komai::rust::settings_load_persisted_secrets_file_from_path(
      path.toStdString(), label, SettingKey::SecretsFileMap));
}

bool
writePersistedSecretsFilePayloadToPath(const QString &path,
                                       const QString &accessToken,
                                       const QMap<QString, QString> &secrets,
                                       bool ownerReadWriteOnly)
{
    return ::komai::rust::settings_write_persisted_secrets_file_to_path(
      path.toStdString(),
      SettingKey::SecretsFileMap,
      accessToken.toStdString(),
      toRustStringMapEntries(secrets),
      ownerReadWriteOnly);
}

} // namespace detail

} // namespace settings::persistence
