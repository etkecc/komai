// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile/ProfileSecrets.h"

#include <QString>
#include <QStringView>

#include "profile/ProfileId.h"

namespace profile_secrets {

QString
normalizedProfileId(QStringView profile)
{
    return profile_id::normalized(profile);
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

bool
deleteAllProfileSecretsFromStoreBlocking(QStringView, staged_load_plan::SecretsProvider)
{
    return true;
}

} // namespace profile_secrets
