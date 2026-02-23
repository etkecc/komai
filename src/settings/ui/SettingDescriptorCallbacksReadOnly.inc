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

static QVariant
sessionProfileValue()
{
    auto i = I;
    if (!i)
        return QCoreApplication::translate("UserSettingsModel", "Default");

    return i->profile().isEmpty() ? QCoreApplication::translate("UserSettingsModel", "Default")
                                  : i->profile();
}

static QVariant
sessionDeviceFingerprintValue()
{
    auto fingerprint = utils::humanReadableFingerprint(olm::client()->identity_keys().ed25519);
    fingerprint.replace(u'\n', u' ');
    return fingerprint.simplified();
}

static QVariant
aboutAppNameValue()
{
    return QStringLiteral("<a href=\"https://github.com/etkecc/komai\">Komai</a> @ ") +
           QString::fromStdString(nheko::version) +
           QStringLiteral(" (<a href=\"https://github.com/etkecc/komai/commit/") +
           QString::fromStdString(nheko::commit_hash) + QStringLiteral("\">") +
           QString::fromStdString(nheko::commit_hash) + QStringLiteral("</a>)");
}

static QVariant
aboutPlatformValue()
{
    return QString::fromStdString(nheko::build_os);
}

static QVariant
aboutBasedOnValue()
{
    return QStringLiteral("<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 "
                          "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
                          "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a>)");
}

static QVariant
aboutMaintainedByValue()
{
    return QStringLiteral("<a href=\"https://etke.cc\">etke.cc</a>");
}
