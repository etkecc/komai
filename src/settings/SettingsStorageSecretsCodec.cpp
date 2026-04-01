// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"

#include "komai-rust-cxxbridge/lib.h"
#include "settings/SettingKeys.h"

namespace settings::storage {

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

} // namespace

QString
encodeSecretsMap(const QMap<QString, QString> &secrets)
{
    const auto encoded =
      ::komai::rust::settings_encode_string_map_yaml(toRustStringMapEntries(secrets));
    return QString::fromStdString(static_cast<std::string>(encoded));
}

QMap<QString, QString>
decodeSecretsMap(const QString &serialized)
{
    return fromRustStringMapEntries(
      ::komai::rust::settings_decode_string_map_yaml(serialized.toStdString()));
}

QString
encodeSecretsFilePayload(const QMap<QString, QString> &secrets)
{
    const auto encoded = ::komai::rust::settings_encode_named_string_map_yaml(
      SettingKey::SecretsFileMap, toRustStringMapEntries(secrets));
    return QString::fromStdString(static_cast<std::string>(encoded));
}

QMap<QString, QString>
decodeSecretsFilePayload(const QString &serialized)
{
    return fromRustStringMapEntries(::komai::rust::settings_decode_named_string_map_yaml(
      serialized.toStdString(), SettingKey::SecretsFileMap));
}

} // namespace settings::storage
