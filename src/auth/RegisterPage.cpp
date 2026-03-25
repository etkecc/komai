// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RegisterPage.h"

#include "LoginPage.h"
#include "logging/Logging.h"

namespace {
QString
normalizeHomeserverInput(QString homeserver)
{
    homeserver = homeserver.trimmed();
    homeserver.remove(u'\r');
    homeserver.remove(u'\n');
    return homeserver;
}

QString
notMigratedMessage()
{
    return RegisterPage::tr("Registration is not migrated to the matrix-sdk backend yet.");
}
}

RegisterPage::RegisterPage(QObject *parent)
  : QObject(parent)
{
}

void
RegisterPage::setError(const QString &err)
{
    registrationError_ = err;
    emit errorChanged();
    registering_ = false;
    emit registeringChanged();
}

void
RegisterPage::setHsError(const QString &err)
{
    hsError_ = err;
    emit hsErrorChanged();
    lookingUpHs_ = false;
    emit lookingUpHsChanged();
}

QString
RegisterPage::initialDeviceName() const
{
    return QString::fromStdString(LoginPage::initialDeviceName_());
}

void
RegisterPage::setServer(const QString &server)
{
    const auto normalized = normalizeHomeserverInput(server);
    if (normalized == lastServer)
        return;

    lastServer = normalized;

    usernameError_.clear();
    emit lookingUpUsernameChanged();

    lookingUpHs_ = true;
    emit lookingUpHsChanged();

    supported_ = false;
    setHsError(notMigratedMessage());
    nhlog::net()->warn("Registration discovery is not migrated to matrix-sdk yet");
}

void
RegisterPage::checkUsername(const QString &name)
{
    Q_UNUSED(name);

    usernameAvailable_   = false;
    usernameUnavailable_ = false;
    usernameError_       = notMigratedMessage();
    lookingUpUsername_   = false;
    emit lookingUpUsernameChanged();
}

void
RegisterPage::startRegistration(const QString &username,
                                const QString &password,
                                const QString &devicename)
{
    Q_UNUSED(username);
    Q_UNUSED(password);
    Q_UNUSED(devicename);

    nhlog::net()->warn("Registration is not migrated to matrix-sdk yet");
    setError(notMigratedMessage());
}

#include "moc_RegisterPage.cpp"
