// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ReCaptcha.h"

#include "logging/Logging.h"
#include "ui/MainWindow.h"

ReCaptcha::ReCaptcha(const QString &session, const QString &context, QObject *parent)
  : QObject{parent}
  , m_session{session}
  , m_context{context}
{
}

void
ReCaptcha::openReCaptcha()
{
    nhlog::ui()->warn("ReCaptcha fallback auth is not migrated to matrix-sdk yet");
    MainWindow::instance()->showNotification(tr(
      "ReCaptcha fallback authentication is not available yet during the matrix-sdk migration."));
}

#include "moc_ReCaptcha.cpp"
