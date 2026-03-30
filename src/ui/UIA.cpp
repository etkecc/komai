// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UIA.h"

#include <algorithm>

#include <QInputDialog>
#include <QTimer>

#include "logging/Logging.h"

namespace {
void
reportUiaUnavailable(UIA *uia, QString action)
{
    nhlog::ui()->warn(
      "Refusing legacy UIA action '{}'; this flow is not migrated to the matrix-sdk backend yet",
      action.toStdString());
    emit uia->error(
      UIA::tr("%1 is not migrated to the matrix-sdk backend yet.").arg(std::move(action)));
}
}

UIA *
UIA::instance()
{
    static UIA uia{nullptr};
    return &uia;
}

void
UIA::continuePassword(const QString &password)
{
    Q_UNUSED(password);
    reportUiaUnavailable(this, tr("Password-based authentication"));
}

void
UIA::continueEmail(const QString &email)
{
    (void)email;
    reportUiaUnavailable(this, tr("Email verification"));
}
void
UIA::continuePhoneNumber(const QString &countryCode, const QString &phoneNumber)
{
    (void)countryCode;
    (void)phoneNumber;
    reportUiaUnavailable(this, tr("Phone-number verification"));
}

void
UIA::continue3pidReceived()
{
    reportUiaUnavailable(this, tr("3PID verification continuation"));
}

void
UIA::submit3pidToken(const QString &token)
{
    (void)token;
    reportUiaUnavailable(this, tr("3PID token submission"));
}

#include "moc_UIA.cpp"
