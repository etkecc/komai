// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/RoomlistModel.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "ui/MainWindow.h"

void
TimelineViewManager::updateCurrentMatrixTimelineSelection()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

    const auto preview = rooms_->currentRoomPreview();
    if (!preview.isMatrixSummary() || rooms_->currentRoom() != nullptr || handleId == 0) {
        clearCurrentMatrixTimeline(handleId != 0);
        return;
    }

    const auto roomId = preview.roomid();
    if (roomId.isEmpty()) {
        clearCurrentMatrixTimeline(handleId != 0);
        return;
    }

    if (activeMatrixTimelineRoomId_ == roomId && matrixTimelineRefreshTimer_ &&
        matrixTimelineRefreshTimer_->isActive()) {
        return;
    }

    QString error;
    if (!komai::MatrixBackendRuntimeService::selectActiveRoomTimeline(handleId, roomId, &error)) {
        nhlog::ui()->warn(
          "Failed to select active matrix-sdk room timeline for '{}' on handle {}: {}",
          roomId.toStdString(),
          handleId,
          error.toStdString());
        clearCurrentMatrixTimeline(false);
        return;
    }

    activeMatrixTimelineRoomId_ = roomId;
    matrixTimelineLoading_      = true;
    emit matrixTimelineStateChanged();

    if (matrixTimelineRefreshTimer_ && !matrixTimelineRefreshTimer_->isActive())
        matrixTimelineRefreshTimer_->start();

    refreshCurrentMatrixTimeline();
}

void
TimelineViewManager::refreshCurrentMatrixTimeline()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        clearCurrentMatrixTimeline(false);
        return;
    }

    QString error;
    const auto items =
      komai::MatrixBackendRuntimeService::fetchActiveRoomTimeline(handleId, &error);
    if (!items) {
        nhlog::ui()->warn("Failed to fetch active matrix-sdk room timeline for '{}' on handle {}: "
                          "{}",
                          activeMatrixTimelineRoomId_.toStdString(),
                          handleId,
                          error.toStdString());
        clearCurrentMatrixTimeline(false);
        return;
    }

    matrixTimelineModel_->replaceItems(*items);

    if (matrixTimelineLoading_) {
        matrixTimelineLoading_ = false;
        emit matrixTimelineStateChanged();
    }
}

void
TimelineViewManager::clearCurrentMatrixTimeline(bool stopBackendTask)
{
    bool stateChanged = false;

    if (matrixTimelineRefreshTimer_ && matrixTimelineRefreshTimer_->isActive())
        matrixTimelineRefreshTimer_->stop();

    if (matrixTimelineLoading_) {
        matrixTimelineLoading_ = false;
        stateChanged           = true;
    }

    if (!activeMatrixTimelineRoomId_.isEmpty()) {
        if (stopBackendTask) {
            const auto *mainWindow = MainWindow::instance();
            const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
            if (handleId != 0) {
                QString error;
                if (!komai::MatrixBackendRuntimeService::selectActiveRoomTimeline(
                      handleId, QString(), &error)) {
                    nhlog::ui()->warn(
                      "Failed to clear active matrix-sdk room timeline on handle {}: {}",
                      handleId,
                      error.toStdString());
                }
            }
        }

        activeMatrixTimelineRoomId_.clear();
        stateChanged = true;
    }

    if (matrixTimelineModel_)
        matrixTimelineModel_->clear();

    if (stateChanged)
        emit matrixTimelineStateChanged();
}
