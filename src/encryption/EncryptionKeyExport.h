// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

/// Local E2EE room-key export/import: writes and reads the
/// passphrase-encrypted key file format shared with Element and Nheko.
/// Surfaced in Settings -> Account -> Encryption keys.
class EncryptionKeyExport final : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    static EncryptionKeyExport *instance();

    // QML singleton entry point. Defined inline so qmltyperegistrar reliably
    // picks it up — the private constructor below ensures QML cannot fall
    // back to default-construction, which would silently create a second
    // instance and misroute signals.
    static EncryptionKeyExport *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool busy() const { return busy_; }

    Q_INVOKABLE void exportKeys(const QUrl &file, const QString &passphrase);
    Q_INVOKABLE void importKeys(const QUrl &file, const QString &passphrase);

signals:
    void busyChanged();
    void exportCompleted(int count);
    void importCompleted(int imported, int total);
    void exportFailed(const QString &error);
    void importFailed(const QString &error);

private:
    explicit EncryptionKeyExport(QObject *parent = nullptr);

    void setBusy(bool busy);

    bool busy_ = false;
};
