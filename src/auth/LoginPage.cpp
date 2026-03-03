// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LoginPage.h"

#include "Logging.h"
#include "MainWindow.h"
#include "MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

LoginPage::LoginPage(QObject *parent)
  : QObject(parent)
  , inferredServerAddress_()
{
    [[maybe_unused]] static auto ignored = qRegisterMetaType<mtx::responses::Login>();

    connect(this, &LoginPage::versionOkCb, this, &LoginPage::versionOk, Qt::QueuedConnection);
    connect(this, &LoginPage::versionErrorCb, this, &LoginPage::versionError, Qt::QueuedConnection);
    connect(
      this,
      &LoginPage::loginOk,
      this,
      [this](const mtx::responses::Login &res) {
          loggingIn_ = false;
          emit loggingInChanged();

          auto *settings                = UserSettings::instance().get();
          const bool hadSessionIdentity = settings->hasPersistedSessionIdentity();

          const auto homeserver = QString::fromStdString(http::client()->server_url());
          const bool persisted  = settings->persistSessionSnapshot(
            UserSettings::SessionSnapshot{.userId = QString::fromStdString(res.user_id.to_string()),
                                           .accessToken = QString::fromStdString(res.access_token),
                                           .deviceId    = QString::fromStdString(res.device_id),
                                           .homeserver  = homeserver});

          if (!persisted) {
              showError(tr("Login failed: server returned incomplete session data."));
              return;
          }

          nhlog::ui()->info(
            "Persisted login session snapshot (user_id='{}', device_id='{}', homeserver='{}')",
            QString::fromStdString(res.user_id.to_string()).toStdString(),
            QString::fromStdString(res.device_id).toStdString(),
            homeserver.toStdString());

          MainWindow::instance()->showChatPage(hadSessionIdentity);
      },
      Qt::QueuedConnection);
}

void
LoginPage::showError(const QString &msg)
{
    loggingIn_ = false;
    emit loggingInChanged();

    error_ = msg;
    emit errorOccurred();
}

#include "moc_LoginPage.cpp"
