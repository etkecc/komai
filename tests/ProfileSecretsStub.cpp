// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCryptographicHash>
#include <QString>

#include "ProfileSecrets.h"

namespace profile_secrets {

QString
normalizedProfileId(QStringView profile)
{
    if (profile.isEmpty() || profile == u"default")
        return QStringLiteral("default");
    return profile.toString();
}

QString
profileHashHex(QStringView profile)
{
    return QString::fromLatin1(
      QCryptographicHash::hash(normalizedProfileId(profile).toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return QStringLiteral("komai.") + profileHashHex(profile) + QStringLiteral(".settings.") +
           keyName.toString();
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
