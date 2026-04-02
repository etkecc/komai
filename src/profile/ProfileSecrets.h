// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringView>

#include <array>
#include <string_view>

namespace profile_secrets {

struct CacheSecretDescriptor
{
    std::string_view name;
    bool internal;
};

enum class SecretStoreBackend
{
    SecureStore,
    File,
};

bool
deleteProfileSecretValueBlocking(const QString &key);

bool
deleteEmptyProfileSecretValueBlocking(const QString &key);

QString
normalizedProfileId(QStringView profile);
QString
settingsSecretStoreKey(QStringView profile, QStringView keyName);
QString
cacheSecretStoreKey(QStringView profile, std::string_view keyName, bool internal);
const std::array<CacheSecretDescriptor, 5> &
cacheSecretDescriptors() noexcept;
const std::array<std::string_view, 1> &
settingsSecretNames() noexcept;
bool
deleteAllProfileSecretsFromStoreBlocking(QStringView profile);
bool
deleteAllProfileSecretsFromStoreBlocking(QStringView profile, SecretStoreBackend backend);
inline bool
deleteAllProfileSecretsFromStoreBlocking(QStringView profile, bool usesFileSecretsProvider)
{
    return deleteAllProfileSecretsFromStoreBlocking(
      profile,
      usesFileSecretsProvider ? SecretStoreBackend::File : SecretStoreBackend::SecureStore);
}
bool
deleteSettingsProfileSecretsFromStoreBlocking(QStringView profile);
bool
deleteCacheProfileSecretsFromStoreBlocking(QStringView profile);

} // namespace profile_secrets
