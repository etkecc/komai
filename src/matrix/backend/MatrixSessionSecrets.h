// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace komai::matrix_backend {

struct PersistedMatrixSessionSecrets
{
    QString storePassphrase;
    QString homeserverUrl;
    QString serializedSession;
};

PersistedMatrixSessionSecrets
loadPersistedMatrixSessionSecrets(const QString &profileId);

bool
savePersistedMatrixSessionSecrets(const QString &profileId,
                                  const PersistedMatrixSessionSecrets &secrets);

void
clearPersistedMatrixSessionSecrets(const QString &profileId);

} // namespace komai::matrix_backend
