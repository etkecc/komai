// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chat/ChatPage.h"

#include "logging/Logging.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

void
ChatPage::bootstrap(QString userid,
                    QString deviceId,
                    QString homeserver,
                    QString token,
                    bool hadSessionIdentity)
{
    (void)userid;
    (void)deviceId;
    (void)homeserver;
    (void)token;
    (void)hadSessionIdentity;

    shuttingDown_ = false;

    if (!(MainWindow::instance() && MainWindow::instance()->matrixBackendHandleId() != 0)) {
        nhlog::ui()->error(
          "Refusing to bootstrap chat page without a resident matrix-sdk runtime handle");
        emit dropToLoginPageCb(
          tr("Matrix backend runtime failed to start for this session. Please log in again."));
        return;
    }

    nhlog::ui()->info("Bootstrapping chat page from resident matrix-sdk runtime only");

    emit initializeEmptyViews();
    getProfileInfo();
    emit contentLoaded();
    emit MainWindow::instance()->reload();
}
