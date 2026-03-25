// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FallbackAuth.h"

#include "logging/Logging.h"
#include "ui/MainWindow.h"

FallbackAuth::FallbackAuth(const QString &session, const QString &authType, QObject *parent)
  : QObject{parent}
  , m_session{session}
  , m_authType{authType}
{
}

void
FallbackAuth::openFallbackAuth()
{
    nhlog::ui()->warn("Fallback auth '{}' is not migrated to matrix-sdk yet",
                      m_authType.toStdString());
    MainWindow::instance()->showNotification(
      tr("Fallback authentication is not available yet during the matrix-sdk migration."));
}

#include "moc_FallbackAuth.cpp"
