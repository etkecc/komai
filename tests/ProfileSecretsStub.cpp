// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>

#include "profile/KeyringEnvironment.h"
#include "profile/ProfileId.h"
#include "profile/ProfileSecrets.h"

namespace profile_secrets {

QString
normalizedProfileId(QStringView profile)
{
    return profile_id::normalized(profile);
}

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return keyring_environment::prefix() + normalizedProfileId(profile) +
           QStringLiteral(".settings.") + keyName.toString();
}

bool
deleteProfileSecretValueBlocking(const QString &key)
{
    (void)key;
    return true;
}

bool
deleteAllProfileSecretsFromStoreBlocking(QStringView profile)
{
    (void)profile;
    return true;
}

bool
deleteAllProfileSecretsFromStoreBlocking(QStringView profile, SecretStoreBackend)
{
    (void)profile;
    return true;
}

} // namespace profile_secrets
