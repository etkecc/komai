// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProfileSecrets.h"

#include <QCryptographicHash>

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
      QCryptographicHash::hash(normalizedProfileId(profile).toUtf8(), QCryptographicHash::Sha256)
        .toHex());
}

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return QStringLiteral("komai.") + profileHashHex(profile) + QStringLiteral(".settings.") +
           keyName.toString();
}

QString
cacheSecretStoreKey(QStringView profile, std::string_view keyName, bool internal)
{
    return QStringLiteral("komai.") + profileHashHex(profile) + QStringLiteral(".") +
           (internal ? QStringLiteral("local_crypto.") : QStringLiteral("matrix.")) +
           QString::fromUtf8(keyName.data(), static_cast<int>(keyName.size()));
}

} // namespace profile_secrets
