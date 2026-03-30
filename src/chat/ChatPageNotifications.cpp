// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chat/ChatPage.h"

#include "logging/Logging.h"
#include "matrix/MatrixSyncUpdate.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"

void
ChatPage::processSyncUi(const komai::NotificationSyncUpdate &notificationUpdate)
{
    static unsigned int prevNotificationCount = 0;
    const auto notificationCount              = notificationUpdate.notificationCount;

    // HACK: If we had less notifications last time we checked, send an alert if the
    // user wanted one. Technically, this may cause an alert to be missed if new ones
    // come in while you are reading old ones. Since the window is almost certainly open
    // in this edge case, that's probably a non-issue.
    // TODO: Replace this once we have proper pushrules support. This is a horrible hack
    if (prevNotificationCount < notificationCount) {
        if (userSettings_->notificationsAttentionOnIncoming() &&
            userSettings_->notificationsAccountEnabled())
            MainWindow::instance()->alert(0);
    }
    prevNotificationCount = notificationCount;
}
