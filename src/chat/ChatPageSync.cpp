// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QPointer>

#include <thread>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"

namespace {
bool
hasMatrixSdkRuntime()
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow && mainWindow->matrixBackendHandleId() != 0;
}

std::optional<uint64_t>
matrixRuntimeHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() == 0)
        return std::nullopt;

    return mainWindow->matrixBackendHandleId();
}
} // namespace

QString
ChatPage::status() const
{
    return statusMessageShadow_.value_or(QString{});
}

void
ChatPage::setStatus(const QString &status)
{
    statusMessageShadow_ = status;

    if (hasMatrixSdkRuntime()) {
        nhlog::net()->info(
          "Presence/status updates via matrix-sdk are not implemented yet; updating local status "
          "shadow only");
        return;
    }

    nhlog::net()->warn(
      "Ignoring presence/status update because no active matrix-sdk runtime exists");
}

bool
ChatPage::shouldBeUnavailable() const
{
    return lastWindowActive.isValid() &&
           lastWindowActive.addSecs(60 * 5) < QDateTime::currentDateTime();
}

mtx::presence::PresenceState
ChatPage::currentPresence() const
{
    switch (userSettings_->networkPresenceStatusPolicy()) {
    case UserSettings::Presence::Online:
        return mtx::presence::online;
    case UserSettings::Presence::Unavailable:
        return mtx::presence::unavailable;
    case UserSettings::Presence::Offline:
        return mtx::presence::offline;
    case UserSettings::Presence::AutomaticPresence:
        if (shouldBeUnavailable())
            return mtx::presence::unavailable;
        else
            return mtx::presence::online;

    default:
        return mtx::presence::online;
    }
}

void
ChatPage::getProfileInfo()
{
    const auto handleId = matrixRuntimeHandleId();
    if (!handleId) {
        nhlog::net()->warn(
          "Cannot retrieve own profile via matrix-sdk runtime because no runtime handle is active");
        return;
    }

    QPointer<ChatPage> guard(this);

    std::thread([guard, handleId = *handleId]() {
        QString error;
        auto result = komai::MatrixBackendRuntimeService::fetchOwnProfile(handleId, &error);

        if (!guard)
            return;

        emit guard->callFunctionOnGuiThread([guard, result = std::move(result), error]() {
            if (!guard || guard->shuttingDown_)
                return;

            if (!result) {
                nhlog::net()->warn("Failed to retrieve own profile info via matrix-sdk runtime "
                                   "handle: {}",
                                   error.toStdString());
                return;
            }

            emit guard->setUserDisplayName(result->displayName);
            emit guard->setUserAvatar(result->avatarUrl);
        });
    }).detach();
}

bool
ChatPage::isRoomActive(const QString &room_id)
{
    return QGuiApplication::focusWindow() && QGuiApplication::focusWindow()->isActive() &&
           MainWindow::instance()->windowForRoom(room_id) == QGuiApplication::focusWindow();
}
