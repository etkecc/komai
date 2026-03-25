// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QUrl>

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

QString
matrixTimelineAttachmentFileName(const QString &suggestedFileName, const QString &itemId)
{
    const auto fileName = QFileInfo(suggestedFileName).fileName().trimmed();
    if (!fileName.isEmpty())
        return fileName;

    const auto trimmedItemId = itemId.trimmed();
    if (!trimmedItemId.isEmpty())
        return trimmedItemId + QStringLiteral(".bin");

    return QStringLiteral("matrix-attachment.bin");
}

QString
matrixTimelineMediaCachePath(const QString &fileName)
{
    auto baseDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (baseDir.isEmpty())
        baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (baseDir.isEmpty())
        baseDir = QDir::tempPath();

    const auto cacheDir = QDir(baseDir).filePath(QStringLiteral("matrix-runtime-media"));
    QDir().mkpath(cacheDir);
    return QDir(cacheDir).filePath(fileName);
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

    clearActiveMatrixReplyState();

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
    bool stateChanged = clearActiveMatrixReplyState();

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
    const auto replyEventId  = matrixTimelineReplyEventId_.trimmed();

    QString error;
    const bool ok =
      replyEventId.isEmpty()
        ? komai::MatrixBackendRuntimeService::sendRoomMessage(handleId,
                                                              activeMatrixTimelineRoomId_,
                                                              plainBody,
                                                              formattedHtml,
                                                              QStringLiteral("text"),
                                                              &error)
        : komai::MatrixBackendRuntimeService::sendRoomReplyMessage(handleId,
                                                                   activeMatrixTimelineRoomId_,
                                                                   replyEventId,
                                                                   plainBody,
                                                                   formattedHtml,
                                                                   QStringLiteral("text"),
                                                                   &error);

    if (!ok) {
        nhlog::ui()->warn("Failed to queue matrix-sdk room {} for '{}' on handle {}: {}",
                          replyEventId.isEmpty() ? "message" : "reply",
                          activeMatrixTimelineRoomId_.toStdString(),
                          handleId,
                          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to send message: %1").arg(error));
        return false;
    }

    if (clearActiveMatrixReplyState())
        emit matrixTimelineStateChanged();

    return true;
}

bool
TimelineViewManager::queueActiveMatrixReply(const QString &eventId,
                                            const QString &senderDisplayName,
                                            const QString &body)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto trimmedSenderDisplayName = senderDisplayName.trimmed();
    const auto trimmedBody              = body.trimmed();
    const auto changed =
      setActiveMatrixReplyState(trimmedEventId, trimmedSenderDisplayName, trimmedBody);
    if (changed)
        emit matrixTimelineStateChanged();
    focusMessageInput();
    return true;
}

void
TimelineViewManager::clearActiveMatrixReply()
{
    if (!clearActiveMatrixReplyState())
        return;

    emit matrixTimelineStateChanged();
    emit replyClosed();
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

bool
TimelineViewManager::openActiveMatrixTimelineMedia(const QString &itemId,
                                                   const QString &suggestedFileName)
{
    const auto trimmedItemId = itemId.trimmed();
    if (trimmedItemId.isEmpty())
        return false;

    const auto fileName = matrixTimelineAttachmentFileName(suggestedFileName, trimmedItemId);
    fetchActiveMatrixTimelineMediaToFile(
      trimmedItemId, matrixTimelineMediaCachePath(fileName), fileName, true);
    return true;
}

bool
TimelineViewManager::saveActiveMatrixTimelineMedia(const QString &itemId,
                                                   const QString &suggestedFileName)
{
    const auto trimmedItemId = itemId.trimmed();
    if (trimmedItemId.isEmpty())
        return false;

    const auto downloadsFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const auto fileName        = matrixTimelineAttachmentFileName(suggestedFileName, trimmedItemId);
    const auto outputPath      = QFileDialog::getSaveFileName(
      nullptr, tr("Save attachment"), downloadsFolder + u'/' + fileName);
    if (outputPath.isEmpty())
        return false;

    fetchActiveMatrixTimelineMediaToFile(trimmedItemId, outputPath, fileName, false);
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

bool
TimelineViewManager::setActiveMatrixReplyState(const QString &eventId,
                                               const QString &senderDisplayName,
                                               const QString &body)
{
    const auto trimmedEventId           = eventId.trimmed();
    const auto trimmedSenderDisplayName = senderDisplayName.trimmed();
    const auto trimmedBody              = body.trimmed();

    if (matrixTimelineReplyEventId_ == trimmedEventId &&
        matrixTimelineReplySenderDisplayName_ == trimmedSenderDisplayName &&
        matrixTimelineReplyBody_ == trimmedBody) {
        return false;
    }

    matrixTimelineReplyEventId_           = trimmedEventId;
    matrixTimelineReplySenderDisplayName_ = trimmedSenderDisplayName;
    matrixTimelineReplyBody_              = trimmedBody;
    emit replyingEventChanged(matrixTimelineReplyEventId_);
    return true;
}

bool
TimelineViewManager::clearActiveMatrixReplyState()
{
    if (matrixTimelineReplyEventId_.isEmpty() && matrixTimelineReplySenderDisplayName_.isEmpty() &&
        matrixTimelineReplyBody_.isEmpty()) {
        return false;
    }

    matrixTimelineReplyEventId_.clear();
    matrixTimelineReplySenderDisplayName_.clear();
    matrixTimelineReplyBody_.clear();
    emit replyingEventChanged(QString());
    return true;
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

void
TimelineViewManager::fetchActiveMatrixTimelineMediaToFile(const QString &itemId,
                                                          const QString &outputPath,
                                                          const QString &userVisibleName,
                                                          bool openAfterSave)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to fetch matrix-sdk timeline media without an active runtime "
                          "handle or selected matrix room");
        if (mainWindow) {
            mainWindow->showNotification(
              tr("Failed to fetch attachment '%1': no active Matrix session").arg(userVisibleName));
        }
        return;
    }

    std::thread([this, handleId, itemId, outputPath, userVisibleName, openAfterSave]() {
        QString error;
        const auto data = komai::MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
          handleId, itemId, 0, 0, false, &error);
        bool ok = data.has_value() && !data->isEmpty();

        if (ok) {
            QFileInfo outputInfo(outputPath);
            QDir().mkpath(outputInfo.absolutePath());

            QFile outputFile(outputPath);
            if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                ok    = false;
                error = outputFile.errorString();
            } else if (outputFile.write(*data) != data->size()) {
                ok    = false;
                error = outputFile.errorString();
            } else {
                outputFile.close();
                utils::markFileAsFromWeb(outputPath);
            }
        }

        QMetaObject::invokeMethod(
          this,
          [this, ok, outputPath, userVisibleName, openAfterSave, error]() {
              auto *mainWindow = MainWindow::instance();
              if (!ok) {
                  nhlog::ui()->warn("Failed to fetch matrix-sdk timeline media '{}' to '{}': {}",
                                    userVisibleName.toStdString(),
                                    outputPath.toStdString(),
                                    error.toStdString());
                  if (mainWindow) {
                      mainWindow->showNotification(
                        tr("Failed to fetch attachment '%1': %2").arg(userVisibleName, error));
                  }
                  return;
              }

              if (openAfterSave && !QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath))) {
                  nhlog::ui()->warn("Failed to open fetched matrix-sdk timeline media '{}'",
                                    outputPath.toStdString());
                  if (mainWindow) {
                      mainWindow->showNotification(
                        tr("Saved attachment '%1' but failed to open it").arg(userVisibleName));
                  }
              }
          },
          Qt::QueuedConnection);
    }).detach();
}
