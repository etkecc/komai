// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProfileSecrets.h"

#include <QString>
#include <QStringView>

namespace profile_secrets {

QString
normalizedProfileId(QStringView profile)
{
    if (profile.isEmpty() || profile == u"default")
        return QStringLiteral("default");

    return profile.toString();
}

bool
deleteProfileSecretValueBlocking(const QString &)
{
    return true;
}

bool
deleteAllProfileSecretsFromStoreBlocking(QStringView)
{
    return true;
}

} // namespace profile_secrets
