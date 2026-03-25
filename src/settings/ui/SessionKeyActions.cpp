// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SessionKeyActions.h"

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextStream>
#include <exception>

#include <mtxclient/crypto/client.hpp>

#include "cache/Cache.h"

namespace settings::ui {

void
importSessionKeys()
{
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString fileName   = QFileDialog::getOpenFileName(
      nullptr, QObject::tr("Open Sessions File"), homeFolder, QLatin1String(""));

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"), file.errorString());
        return;
    }

    const auto bin     = file.peek(file.size());
    const auto payload = std::string(bin.data(), bin.size());

    bool ok = false;
    const auto password =
      QInputDialog::getText(nullptr,
                            QObject::tr("File Password"),
                            QObject::tr("Enter the passphrase to decrypt the file:"),
                            QLineEdit::Password,
                            QLatin1String(""),
                            &ok);
    if (!ok)
        return;

    if (password.isEmpty()) {
        QMessageBox::warning(
          nullptr, QObject::tr("Error"), QObject::tr("The password cannot be empty"));
        return;
    }

    try {
        auto sessions = mtx::crypto::decrypt_exported_sessions(payload, password.toStdString());
        cache::importSessionKeys(std::move(sessions));
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, QObject::tr("Error"), e.what());
    }
}

void
exportSessionKeys()
{
    bool ok = false;
    const auto password =
      QInputDialog::getText(nullptr,
                            QObject::tr("File Password"),
                            QObject::tr("Enter passphrase to encrypt your session keys:"),
                            QLineEdit::Password,
                            QLatin1String(""),
                            &ok);
    if (!ok)
        return;

    if (password.isEmpty()) {
        QMessageBox::warning(
          nullptr, QObject::tr("Error"), QObject::tr("The password cannot be empty"));
        return;
    }

    const auto repeatedPassword = QInputDialog::getText(nullptr,
                                                        QObject::tr("Repeat File Password"),
                                                        QObject::tr("Repeat the passphrase:"),
                                                        QLineEdit::Password,
                                                        QLatin1String(""),
                                                        &ok);
    if (!ok)
        return;

    if (password != repeatedPassword) {
        QMessageBox::warning(nullptr, QObject::tr("Error"), QObject::tr("Passwords don't match"));
        return;
    }

    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString fileName   = QFileDialog::getSaveFileName(
      nullptr, QObject::tr("File to save the exported session keys"), homeFolder);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"), file.errorString());
        return;
    }

    try {
        const auto encryptedBlob = mtx::crypto::encrypt_exported_sessions(
          cache::exportSessionKeys(), password.toStdString());
        const auto b64 = QString::fromStdString(mtx::crypto::bin2base64(encryptedBlob));

        const QString prefix(QStringLiteral("-----BEGIN MEGOLM SESSION DATA-----"));
        const QString suffix(QStringLiteral("-----END MEGOLM SESSION DATA-----"));
        const QString newline(QStringLiteral("\n"));
        QTextStream out(&file);
        out << prefix << newline << b64 << newline << suffix << newline;
        file.close();
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, QObject::tr("Error"), e.what());
    }
}

void
requestCrossSigningSecrets()
{
    QMessageBox::information(nullptr,
                             QObject::tr("Not migrated yet"),
                             QObject::tr("Cross-signing secret request has not been migrated to "
                                         "the matrix-sdk backend yet."));
}

void
downloadCrossSigningSecrets()
{
    QMessageBox::information(nullptr,
                             QObject::tr("Not migrated yet"),
                             QObject::tr("Cross-signing secret download has not been migrated to "
                                         "the matrix-sdk backend yet."));
}

} // namespace settings::ui
