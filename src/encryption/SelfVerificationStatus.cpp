// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SelfVerificationStatus.h"

#include "logging/Logging.h"

namespace {
QString
notMigratedMessage()
{
    return SelfVerificationStatus::tr(
      "Encryption setup and self-verification are not migrated to the matrix-sdk backend yet.");
}
}

SelfVerificationStatus::SelfVerificationStatus(QObject *o)
  : QObject(o)
{
    status_                     = AllVerified;
    hasSSSS_                    = false;
    canVerifyWithAnotherDevice_ = false;
}

void
SelfVerificationStatus::setupCrosssigning(bool useSSSS,
                                          const QString &password,
                                          bool encryptionBackupOnlineEnabled)
{
    Q_UNUSED(useSSSS);
    Q_UNUSED(password);
    Q_UNUSED(encryptionBackupOnlineEnabled);
    nhlog::crypto()->warn("Self-verification setup is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
}

bool
SelfVerificationStatus::verifyMasterKey()
{
    nhlog::crypto()->warn("Self-verification master-key flow is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
    return false;
}

void
SelfVerificationStatus::verifyMasterKeyWithPassphrase()
{
    nhlog::crypto()->warn(
      "Self-verification master-key recovery flow is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
}

void
SelfVerificationStatus::verifyUnverifiedDevices()
{
    nhlog::crypto()->warn("Device verification is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
}

void
SelfVerificationStatus::setupEncryptionBackup()
{
    nhlog::crypto()->warn("Encryption backup setup is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
}

void
SelfVerificationStatus::resetEncryptionIdentity()
{
    nhlog::crypto()->warn("Encryption identity reset is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
}

void
SelfVerificationStatus::promptCurrentVerificationAction()
{
    emit setupFailed(notMigratedMessage());
}

void
SelfVerificationStatus::invalidate()
{
    if (status_ != AllVerified) {
        status_ = AllVerified;
        emit statusChanged();
    }

    if (hasSSSS_) {
        hasSSSS_ = false;
        emit hasSSSSChanged();
    }

    if (canVerifyWithAnotherDevice_) {
        canVerifyWithAnotherDevice_ = false;
        emit canVerifyWithAnotherDeviceChanged();
    }
}

#include "moc_SelfVerificationStatus.cpp"
