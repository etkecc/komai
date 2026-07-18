// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixSessionSecrets.h"
#include "komai-rust-cxxbridge/ffi.h"

namespace komai::matrix_backend {

PersistedMatrixSessionSecrets
loadPersistedMatrixSessionSecrets(const QString &profileId)
{
    const auto persisted =
      ::komai::rust::settings_load_persisted_matrix_session_secrets_for_profile(
        profileId.toStdString());

    return {
      .storePassphrase =
        QString::fromStdString(static_cast<std::string>(persisted.store_passphrase)),
      .homeserverUrl = QString::fromStdString(static_cast<std::string>(persisted.homeserver_url)),
      .serializedSession =
        QString::fromStdString(static_cast<std::string>(persisted.serialized_session)),
    };
}

bool
savePersistedMatrixSessionSecrets(const QString &profileId,
                                  const PersistedMatrixSessionSecrets &secrets)
{
    return ::komai::rust::settings_save_persisted_matrix_session_secrets_for_profile(
      profileId.toStdString(),
      secrets.storePassphrase.toStdString(),
      secrets.homeserverUrl.toStdString(),
      secrets.serializedSession.toStdString());
}

void
clearPersistedMatrixSessionSecrets(const QString &profileId)
{
    (void)::komai::rust::settings_clear_persisted_matrix_session_secrets_for_profile(
      profileId.toStdString());
}

} // namespace komai::matrix_backend
