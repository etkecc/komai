// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile/ProfileSecrets.h"

#include <array>

#include <mtx/secret_storage.hpp>

#include "profile/ProfileId.h"

namespace profile_secrets {

const std::array<CacheSecretDescriptor, 5> &
cacheSecretDescriptors() noexcept
{
    static const std::array<CacheSecretDescriptor, 5> descriptors{
      CacheSecretDescriptor{"pickle_secret", true},
      CacheSecretDescriptor{mtx::secret_storage::secrets::cross_signing_master, false},
      CacheSecretDescriptor{mtx::secret_storage::secrets::cross_signing_self_signing, false},
      CacheSecretDescriptor{mtx::secret_storage::secrets::cross_signing_user_signing, false},
      CacheSecretDescriptor{mtx::secret_storage::secrets::megolm_backup_v1, false}};
    return descriptors;
}

const std::array<std::string_view, 1> &
settingsSecretNames() noexcept
{
    static const std::array<std::string_view, 1> names{"session.secrets"};
    return names;
}

QString
normalizedProfileId(QStringView profile)
{
    return profile_id::normalized(profile);
}

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return QStringLiteral("komai.") + normalizedProfileId(profile) +
           QStringLiteral(".settings.") + keyName.toString();
}

QString
cacheSecretStoreKey(QStringView profile, std::string_view keyName, bool internal)
{
    return QStringLiteral("komai.") + normalizedProfileId(profile) + QStringLiteral(".") +
           (internal ? QStringLiteral("local_crypto.") : QStringLiteral("matrix.")) +
           QString::fromUtf8(keyName.data(), static_cast<int>(keyName.size()));
}

bool
deleteProfileSecretValueBlocking(const QString &)
{
    return true;
}

bool
deleteEmptyProfileSecretValueBlocking(const QString &)
{
    return true;
}

bool
deleteAllProfileSecretsFromStoreBlocking(QStringView)
{
    return true;
}

bool
deleteAllProfileSecretsFromStoreBlocking(QStringView, staged_load_plan::SecretsProvider)
{
    return true;
}

bool
deleteSettingsProfileSecretsFromStoreBlocking(QStringView)
{
    return true;
}

bool
deleteCacheProfileSecretsFromStoreBlocking(QStringView)
{
    return true;
}

} // namespace profile_secrets
