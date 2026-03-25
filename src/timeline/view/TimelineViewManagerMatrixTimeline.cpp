// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>

#include <thread>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/CommunitiesModel.h"
#include "timeline/RoomlistModel.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {
QString
matrixMessageFormattedHtml(const QString &body)
{
    auto *chatPage       = ChatPage::instance();
    const auto *settings = chatPage ? chatPage->userSettings().get() : nullptr;
    if (!settings || !settings->composerInputMarkdownToHtmlEnabled())
        return {};

    const auto html        = utils::markdownToHtml(body, false);
    const auto trimmedBody = body.trimmed();

    if (html.contains(u'<') || trimmedBody.contains(u'\n') || trimmedBody.contains(u'\\'))
        return html;

    return {};
}
}

void
TimelineViewManager::updateCurrentMatrixTimelineSelection()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

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

    if (activeMatrixTimelineRoomId_ == roomId) {
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
    if (matrixTimelineModel_)
        matrixTimelineModel_->clear();
    emit matrixTimelineStateChanged();
}

void
TimelineViewManager::refreshCurrentMatrixTimeline()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

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

bool
TimelineViewManager::sendActiveMatrixTextMessage(const QString &body)
{
    const auto plainBody = body.trimmed();
    if (plainBody.isEmpty())
        return false;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to send matrix-sdk room message without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto formattedHtml = matrixMessageFormattedHtml(body);

    QString error;
    if (!komai::MatrixBackendRuntimeService::sendRoomMessage(handleId,
                                                             activeMatrixTimelineRoomId_,
                                                             plainBody,
                                                             formattedHtml,
                                                             QStringLiteral("text"),
                                                             &error)) {
        nhlog::ui()->warn("Failed to queue matrix-sdk room message for '{}' on handle {}: {}",
                          activeMatrixTimelineRoomId_.toStdString(),
                          handleId,
                          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to send message: %1").arg(error));
        return false;
    }

    return true;
}

bool
TimelineViewManager::openActiveMatrixAttachmentSelection()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to queue matrix-sdk room attachment without an active "
                          "runtime handle or selected matrix room");
        return false;
    }

    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QStringList filePaths =
      QFileDialog::getOpenFileNames(nullptr, tr("Select file(s)"), homeFolder, tr("All Files (*)"));
    if (filePaths.isEmpty())
        return false;

    QMimeDatabase mimeDatabase;
    for (const auto &filePath : filePaths) {
        const auto mimeType = mimeDatabase.mimeTypeForFile(filePath).name();
        pendingMatrixAttachments_.push_back(PendingMatrixAttachment{
          .handleId = handleId,
          .roomId   = activeMatrixTimelineRoomId_,
          .filePath = filePath,
          .mimeType = mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType,
        });
    }

    emit matrixTimelineStateChanged();
    startNextPendingMatrixAttachment();
    focusMessageInput();
    return true;
}

void
TimelineViewManager::handleMatrixBackendRoomListSnapshotUpdated(std::uint64_t handleId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    rooms_->refreshMatrixBackendRooms();
    communities_->initializeSidebar();
}

void
TimelineViewManager::handleMatrixBackendRoomTimelineSnapshotUpdated(std::uint64_t handleId,
                                                                    const QString &roomId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    if (activeMatrixTimelineRoomId_ != roomId)
        return;

    refreshCurrentMatrixTimeline();
}

bool
TimelineViewManager::paginateActiveMatrixTimelineBackwards(int pageSize)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to paginate matrix-sdk room timeline without an active "
                          "runtime handle or selected matrix room");
        return false;
    }

    const auto clampedPageSize = static_cast<uint16_t>(std::clamp(pageSize, 0, 500));

    QString error;
    if (!komai::MatrixBackendRuntimeService::paginateActiveRoomTimelineBackwards(
          handleId, clampedPageSize, &error)) {
        nhlog::ui()->warn(
          "Failed to paginate matrix-sdk room timeline backwards for '{}' on handle {}: {}",
          activeMatrixTimelineRoomId_.toStdString(),
          handleId,
          error.toStdString());
        return false;
    }

    return true;
}

void
TimelineViewManager::startNextPendingMatrixAttachment()
{
    if (matrixAttachmentUploadInFlight_ || pendingMatrixAttachments_.empty())
        return;

    const auto attachment           = pendingMatrixAttachments_.front();
    matrixAttachmentUploadInFlight_ = true;
    emit matrixTimelineStateChanged();

    std::thread([this, attachment]() {
        QString error;
        const bool ok = komai::MatrixBackendRuntimeService::sendRoomAttachment(
          attachment.handleId, attachment.roomId, attachment.filePath, attachment.mimeType, &error);

        QMetaObject::invokeMethod(
          this,
          [this, attachment, ok, error]() mutable {
              finishPendingMatrixAttachment(ok, attachment, error);
          },
          Qt::QueuedConnection);
    }).detach();
}

void
TimelineViewManager::finishPendingMatrixAttachment(bool ok,
                                                   const PendingMatrixAttachment &attachment,
                                                   QString error)
{
    if (!pendingMatrixAttachments_.empty() &&
        pendingMatrixAttachments_.front().filePath == attachment.filePath &&
        pendingMatrixAttachments_.front().roomId == attachment.roomId &&
        pendingMatrixAttachments_.front().handleId == attachment.handleId) {
        pendingMatrixAttachments_.pop_front();
    }

    matrixAttachmentUploadInFlight_ = false;
    emit matrixTimelineStateChanged();

    if (!ok) {
        nhlog::ui()->warn("Failed to send matrix-sdk room attachment '{}' for '{}' on handle {}: "
                          "{}",
                          attachment.filePath.toStdString(),
                          attachment.roomId.toStdString(),
                          attachment.handleId,
                          error.toStdString());
        if (auto *mainWindow = MainWindow::instance()) {
            const auto filename = QFileInfo(attachment.filePath).fileName();
            mainWindow->showNotification(
              tr("Failed to send attachment '%1': %2").arg(filename, error));
        }
    }

    startNextPendingMatrixAttachment();
}
