// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>

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
    return QStringLiteral("komai.") + normalizedProfileId(profile) +
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

} // namespace profile_secrets
