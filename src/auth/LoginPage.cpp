// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LoginPage.h"

#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"

LoginPage::LoginPage(QObject *parent)
  : QObject(parent)
  , inferredServerAddress_()
{
    [[maybe_unused]] static auto ignored = qRegisterMetaType<komai::MatrixLoginResult>();

    connect(this, &LoginPage::versionOkCb, this, &LoginPage::versionOk, Qt::QueuedConnection);
    connect(this, &LoginPage::versionErrorCb, this, &LoginPage::versionError, Qt::QueuedConnection);
    connect(
      this,
      &LoginPage::loginOk,
      this,
      [this](const komai::MatrixLoginResult &res) {
          startupRestoreHandoffActive_ = false;
          loggingIn_                   = false;
          emit loggingInChanged();

          auto *settings                = UserSettings::instance().get();
          const bool hadSessionIdentity = settings->hasPersistedSessionIdentity();

          const bool persisted = settings->persistSessionSnapshot(UserSettings::SessionSnapshot{
            .userId      = res.userId,
            .accessToken = res.accessToken,
            .deviceId    = res.deviceId,
            .homeserver  = res.homeserverUrl,
          });

          if (!persisted) {
              showError(tr("Login failed: server returned incomplete session data."));
              return;
          }

          nhlog::ui()->info(
            "Persisted login session snapshot (user_id='{}', device_id='{}', homeserver='{}')",
            res.userId.toStdString(),
            res.deviceId.toStdString(),
            res.homeserverUrl.toStdString());

          auto *mainWindow = MainWindow::instance();
          mainWindow->showStartupRestorePage();
          QMetaObject::invokeMethod(
            mainWindow,
            [mainWindow, hadSessionIdentity] { mainWindow->showChatPage(hadSessionIdentity); },
            Qt::QueuedConnection);
      },
      Qt::QueuedConnection);
}

void
LoginPage::beginStartupRestoreHandoff()
{
    if (startupRestoreHandoffActive_)
        return;

    startupRestoreHandoffActive_ = true;

    if (auto *mainWindow = MainWindow::instance())
        mainWindow->showStartupRestorePage();
}

void
LoginPage::showError(const QString &msg)
{
    const bool returnToLoginPage = startupRestoreHandoffActive_;
    startupRestoreHandoffActive_ = false;
    loggingIn_                   = false;
    emit loggingInChanged();

    error_ = msg;
    emit errorOccurred();

    if (returnToLoginPage) {
        if (auto *mainWindow = MainWindow::instance())
            emit mainWindow->switchToLoginPage(msg);
    }
}

#include "moc_LoginPage.cpp"
