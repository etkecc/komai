// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
encryptionOnlineBackupKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value();
}

static QVariant
encryptionSelfSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing).has_value();
}

static QVariant
encryptionUserSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing).has_value();
}

static QVariant
encryptionMasterSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_master).has_value();
}
