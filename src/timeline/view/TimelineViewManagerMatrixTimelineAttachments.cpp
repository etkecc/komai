// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <QBuffer>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include "chat/ChatPage.h"
#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "timeline/TimelineEventTypes.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "timeline/view/TimelineViewManagerMatrixTimelineInternal.h"
#include "ui/MainWindow.h"
#include "ui/NotificationAction.h"
#include "utils/MediaIcons.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

using namespace komai::timeline::view::internal;

QVariantList
TimelineViewManager::matrixTimelineAttachments() const
{
    QVariantList attachments;
    attachments.reserve(matrixPendingAttachmentItems_.size());

    for (auto *attachment : matrixPendingAttachmentItems_) {
        if (attachment)
            attachments.push_back(QVariant::fromValue(attachment));
    }

    return attachments;
}
int
TimelineViewManager::matrixTimelineAttachmentCount() const
{
    return matrixPendingAttachmentItems_.size();
}
bool
TimelineViewManager::openActiveMatrixAttachmentSelection()
{
    const auto targetRoomId = activeMatrixTimelineRoomId_.trimmed();
    if (targetRoomId.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to queue matrix-sdk room attachment without a selected room");
        return false;
    }

    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QStringList filePaths =
      QFileDialog::getOpenFileNames(nullptr, tr("Select file(s)"), homeFolder, tr("All Files (*)"));
    if (filePaths.isEmpty())
        return false;

    return stageMatrixAttachmentsForRoom(targetRoomId, filePaths);
}
bool
TimelineViewManager::tryPasteClipboardAttachment(bool strict)
{
    const auto targetRoomId = activeMatrixTimelineRoomId_.trimmed();
    if (targetRoomId.isEmpty())
        return false;

    auto *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return false;

    const auto *md = strict ? clipboard->mimeData(QClipboard::Selection)
                            : clipboard->mimeData(QClipboard::Clipboard);
    if (!md)
        return false;

    komai::logging::ui()->info("Clipboard paste: formats=[{}], hasImage={}",
                               md->formats().join(QStringLiteral(", ")).toStdString(),
                               md->hasImage());

    const auto formats = md->formats().filter(QStringLiteral("/"));
    const auto image   = formats.filter(QStringLiteral("image/"), Qt::CaseInsensitive);
    const auto audio   = formats.filter(QStringLiteral("audio/"), Qt::CaseInsensitive);
    const auto video   = formats.filter(QStringLiteral("video/"), Qt::CaseInsensitive);

    // Helper: save raw MIME bytes to a temp file and stage them.
    auto stageFromMimeData = [&](const QString &mimeType) -> bool {
        const auto data = md->data(mimeType);
        if (data.isEmpty())
            return false;

        QMimeDatabase db;
        const auto suffix   = db.mimeTypeForName(mimeType).preferredSuffix();
        const auto tempDir  = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const auto filePath = tempDir + QStringLiteral("/komai-paste-") +
                              QUuid::createUuid().toString(QUuid::Id128) +
                              (suffix.isEmpty() ? QString{} : QStringLiteral(".") + suffix);

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            komai::logging::ui()->warn("Failed to create temp file for clipboard paste: {}",
                                       filePath.toStdString());
            return false;
        }
        file.write(data);
        file.close();

        return stageMatrixAttachmentsForRoom(targetRoomId, {filePath});
    };

    if (md->hasImage()) {
        if (formats.contains(QStringLiteral("image/svg+xml"), Qt::CaseInsensitive)) {
            return stageFromMimeData(QStringLiteral("image/svg+xml"));
        } else if (formats.contains(QStringLiteral("image/png"), Qt::CaseInsensitive)) {
            return stageFromMimeData(QStringLiteral("image/png"));
        } else if (image.empty()) {
            // Convert generic image data to PNG.
            QByteArray ba;
            QBuffer buffer(&ba);
            buffer.open(QIODevice::WriteOnly);
            qvariant_cast<QImage>(md->imageData()).save(&buffer, "PNG");
            buffer.close();

            const auto tempDir  = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            const auto filePath = tempDir + QStringLiteral("/komai-paste-") +
                                  QUuid::createUuid().toString(QUuid::Id128) +
                                  QStringLiteral(".png");

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly))
                return false;
            file.write(ba);
            file.close();

            return stageMatrixAttachmentsForRoom(targetRoomId, {filePath});
        } else {
            return stageFromMimeData(image.first());
        }
    } else if (!audio.empty()) {
        return stageFromMimeData(audio.first());
    } else if (!video.empty()) {
        return stageFromMimeData(video.first());
    } else if (md->hasUrls()) {
        QStringList filePaths;
        for (const auto &url : md->urls()) {
            if (url.isLocalFile())
                filePaths.push_back(url.toLocalFile());
        }
        if (!filePaths.isEmpty())
            return stageMatrixAttachmentsForRoom(targetRoomId, filePaths);
    } else if (md->hasFormat(QStringLiteral("x-special/gnome-copied-files"))) {
        auto data = md->data(QStringLiteral("x-special/gnome-copied-files")).split('\n');
        if (data.size() < 2)
            return false;

        QStringList filePaths;
        for (int i = 1; i < data.size(); ++i) {
            QUrl url{QString::fromUtf8(data[i])};
            if (url.isLocalFile())
                filePaths.push_back(url.toLocalFile());
        }
        if (!filePaths.isEmpty())
            return stageMatrixAttachmentsForRoom(targetRoomId, filePaths);
    }

    // No non-text content found — let Qt handle the normal text paste.
    return false;
}
bool
TimelineViewManager::stageMatrixAttachmentsForRoom(const QString &roomId,
                                                   const QStringList &filePaths)
{
    auto *mainWindow        = MainWindow::instance();
    const auto handleId     = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    const auto targetRoomId = roomId.trimmed();
    if (handleId == 0 || targetRoomId.isEmpty()) {
        komai::logging::ui()->warn("Refusing to queue matrix-sdk room attachment without an active "
                                   "runtime handle or selected matrix room (room='{}')",
                                   targetRoomId.toStdString());
        return false;
    }

    if (!matrixTimelineEditEventId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to stage matrix-sdk room attachments while editing an existing message");
        if (mainWindow) {
            mainWindow->showNotification(
              tr("Finish editing the current message before attaching files."));
        }
        return false;
    }

    QMimeDatabase mimeDatabase;
    QStringList normalizedFilePaths;
    normalizedFilePaths.reserve(filePaths.size());

    for (const auto &filePath : filePaths) {
        const QFileInfo info(filePath);
        if (!info.exists()) {
            komai::logging::ui()->warn(
              "stageMatrixAttachmentsForRoom: rejecting '{}' (path not visible to process; "
              "under Flatpak this typically means the sandbox lacks read access to the source "
              "location -- see https://github.com/etkecc/komai/issues/118)",
              filePath.toStdString());
            continue;
        }
        if (!info.isFile()) {
            komai::logging::ui()->warn(
              "stageMatrixAttachmentsForRoom: rejecting '{}' (not a regular file)",
              filePath.toStdString());
            continue;
        }

        const auto absoluteFilePath = info.absoluteFilePath();
        if (normalizedFilePaths.contains(absoluteFilePath))
            continue;

        normalizedFilePaths.push_back(absoluteFilePath);
        const auto mimeType = mimeDatabase.mimeTypeForFile(absoluteFilePath).name();
        const auto effectiveMimeType =
          mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType;
        const auto fileName = info.fileName();
        pendingMatrixAttachments_.push_back(PendingMatrixAttachment{
          .handleId     = handleId,
          .roomId       = targetRoomId,
          .filePath     = absoluteFilePath,
          .filename     = fileName,
          .body         = {},
          .replyEventId = {},
          .threadId     = {},
          .mimeType     = effectiveMimeType,
          .durationMs   = 0,
          .isVoice      = false,
          .waveform     = {},
        });
        matrixPendingAttachmentItems_.push_back(new MatrixPendingAttachmentUpload(
          absoluteFilePath,
          fileName,
          effectiveMimeType,
          utils::fileTypeIconSource(effectiveMimeType),
          matrixPendingAttachmentThumbnail(absoluteFilePath, effectiveMimeType),
          this));
    }

    if (normalizedFilePaths.isEmpty()) {
        if (mainWindow) {
            mainWindow->showNotification(
              tr("Only existing local files can be attached by drag and drop."));
        }
        return false;
    }

    emit matrixTimelineStateChanged();
    focusMessageInput();
    return true;
}
bool
TimelineViewManager::sendActiveMatrixAttachments()
{
    if (pendingMatrixAttachments_.empty() || matrixAttachmentUploadInFlight_)
        return false;

    const auto replyEventId = matrixTimelineReplyEventId_.trimmed();
    const auto threadId     = matrixTimelineThreadEventId_.trimmed();
    if (!replyEventId.isEmpty() || !threadId.isEmpty()) {
        for (auto &attachment : pendingMatrixAttachments_) {
            if (attachment.replyEventId.isEmpty())
                attachment.replyEventId = replyEventId;
            if (attachment.threadId.isEmpty())
                attachment.threadId = threadId;
        }
    }

    bool clearedState = clearActiveMatrixReplyState();
    startNextPendingMatrixAttachment();
    if (clearedState)
        emit replyClosed();
    return true;
}
bool
TimelineViewManager::stageVoiceRecording(const QString &filePath)
{
    const auto targetRoomId = activeMatrixTimelineRoomId_.trimmed();
    if (targetRoomId.isEmpty() || filePath.isEmpty()) {
        komai::logging::ui()->warn("stageVoiceRecording: missing room or file path");
        return false;
    }

    return stageMatrixAttachmentsForRoom(targetRoomId, {filePath});
}
void
TimelineViewManager::setActiveAttachmentDurationMs(uint64_t durationMs)
{
    for (auto &attachment : pendingMatrixAttachments_)
        attachment.durationMs = durationMs;
}
void
TimelineViewManager::setActiveAttachmentVoiceWaveform(const QList<float> &waveform)
{
    for (auto &attachment : pendingMatrixAttachments_) {
        attachment.isVoice  = true;
        attachment.waveform = waveform;
    }
}
bool
TimelineViewManager::stageAndSendVoiceRecording(const QString &filePath, int durationMs)
{
    const auto targetRoomId = activeMatrixTimelineRoomId_.trimmed();
    if (targetRoomId.isEmpty() || filePath.isEmpty()) {
        komai::logging::ui()->warn("stageAndSendVoiceRecording: missing room or file path");
        return false;
    }

    if (!stageMatrixAttachmentsForRoom(targetRoomId, {filePath}))
        return false;

    for (auto &attachment : pendingMatrixAttachments_) {
        attachment.isVoice    = true;
        attachment.durationMs = static_cast<uint64_t>(std::max(0, durationMs));
    }

    return sendActiveMatrixAttachments();
}
void
TimelineViewManager::clearActiveMatrixAttachments()
{
    if (pendingMatrixAttachments_.empty() && matrixPendingAttachmentItems_.empty())
        return;

    pendingMatrixAttachments_.clear();
    for (auto *attachment : matrixPendingAttachmentItems_) {
        if (attachment)
            attachment->deleteLater();
    }
    matrixPendingAttachmentItems_.clear();

    emit matrixTimelineStateChanged();
}
void
TimelineViewManager::removeActiveMatrixAttachment(int index)
{
    if (index < 0 || index >= matrixPendingAttachmentItems_.size())
        return;

    pendingMatrixAttachments_.erase(pendingMatrixAttachments_.begin() + index);
    auto *attachment = matrixPendingAttachmentItems_.takeAt(index);
    if (attachment)
        attachment->deleteLater();

    emit matrixTimelineStateChanged();
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

    // The dialog must stay non-blocking: a static getSaveFileName() spins a
    // nested event loop while the invoking QML signal handler is still on the
    // stack, and message dialogs delete themselves shortly after closing —
    // destroying an object mid-handler aborts the application.
    auto *dialog = new QFileDialog(nullptr, tr("Save attachment"), downloadsFolder);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->selectFile(fileName);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::ApplicationModal);
    connect(dialog,
            &QFileDialog::fileSelected,
            this,
            [this, trimmedItemId, fileName](const QString &outputPath) {
                if (!outputPath.isEmpty())
                    fetchActiveMatrixTimelineMediaToFile(
                      trimmedItemId, outputPath, fileName, false);
            });
    dialog->show();
    return true;
}
bool
TimelineViewManager::copyActiveMatrixTimelineMedia(const QString &itemId)
{
    const auto trimmedItemId = itemId.trimmed();
    if (trimmedItemId.isEmpty())
        return false;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty() || !matrixTimelineModel_) {
        komai::logging::ui()->warn(
          "Refusing to copy matrix-sdk timeline media without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto item = matrixTimelineModel_->itemByEventId(trimmedItemId);
    if (!item) {
        komai::logging::ui()->warn("Refusing to copy unknown matrix-sdk room event '{}' in '{}'",
                                   trimmedItemId.toStdString(),
                                   activeMatrixTimelineRoomId_.toStdString());
        return false;
    }

    const QString mimeType = item->mimeType.trimmed();
    const auto eventType =
      qml_mtx_events::matrixTimelineEventType(item->itemKind, item->matrixEventType);
    const bool isImage = eventType == qml_mtx_events::EventType::ImageMessage ||
                         eventType == qml_mtx_events::EventType::Sticker;

    std::thread([this, handleId, trimmedItemId, mimeType, isImage]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const auto data = komai::MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
          context, handleId, trimmedItemId, 0, 0, false, &error);

        if (!data.has_value() || data->isEmpty()) {
            QMetaObject::invokeMethod(
              this,
              [trimmedItemId, error]() {
                  komai::logging::ui()->warn(
                    "Failed to fetch matrix-sdk timeline media '{}' for clipboard copy: {}",
                    trimmedItemId.toStdString(),
                    error.toStdString());
                  if (auto *mainWindow = MainWindow::instance()) {
                      mainWindow->showNotification(
                        tr("Failed to copy attachment: %1")
                          .arg(error.isEmpty() ? tr("download failed") : error));
                  }
              },
              Qt::QueuedConnection);
            return;
        }

        QImage image;
        if (isImage) {
            try {
                image = utils::readImage(*data);
            } catch (const std::exception &e) {
                komai::logging::ui()->warn("Error decoding image for clipboard copy: {}", e.what());
            }
        }

        QMetaObject::invokeMethod(
          this,
          [bytes = *data, mimeType, image = std::move(image)]() {
              auto *clipContents = new QMimeData();
              if (!mimeType.isEmpty())
                  clipContents->setData(mimeType, bytes);
              if (!image.isNull())
                  clipContents->setImageData(image);
              QGuiApplication::clipboard()->setMimeData(clipContents);
          },
          Qt::QueuedConnection);
    }).detach();

    return true;
}
void
TimelineViewManager::startNextPendingMatrixAttachment()
{
    if (matrixAttachmentUploadInFlight_ || pendingMatrixAttachments_.empty())
        return;

    auto attachment = pendingMatrixAttachments_.front();
    if (!matrixPendingAttachmentItems_.isEmpty() && matrixPendingAttachmentItems_.front()) {
        const auto *item    = matrixPendingAttachmentItems_.front();
        attachment.filename = item->filename().trimmed();
        attachment.body     = item->body().trimmed();
    }
    matrixAttachmentUploadInFlight_ = true;
    emit matrixTimelineStateChanged();

    const bool stripImageMetadata =
      UserSettings::instance()->composerAttachmentsStripImageMetadata();
    const bool useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();

    std::thread([this, attachment, stripImageMetadata, useMarkdownFormatting]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const bool ok =
          komai::MatrixBackendRuntimeService::sendRoomAttachment(context,
                                                                 attachment.handleId,
                                                                 attachment.roomId,
                                                                 attachment.filePath,
                                                                 attachment.filename,
                                                                 attachment.body,
                                                                 useMarkdownFormatting,
                                                                 attachment.replyEventId,
                                                                 attachment.threadId,
                                                                 attachment.mimeType,
                                                                 attachment.durationMs,
                                                                 attachment.isVoice,
                                                                 attachment.waveform,
                                                                 stripImageMetadata,
                                                                 &error)
            .has_value();

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
    if (!matrixPendingAttachmentItems_.isEmpty()) {
        auto *pendingItem = matrixPendingAttachmentItems_.takeFirst();
        if (pendingItem)
            pendingItem->deleteLater();
    }

    matrixAttachmentUploadInFlight_ = false;
    emit matrixTimelineStateChanged();

    if (!ok) {
        komai::logging::ui()->warn(
          "Failed to send matrix-sdk room attachment '{}' for '{}' on handle {}: "
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
        komai::logging::ui()->warn(
          "Refusing to fetch matrix-sdk timeline media without an active runtime "
          "handle or selected matrix room");
        if (mainWindow) {
            mainWindow->showNotification(
              tr("Failed to fetch attachment '%1': no active Matrix session").arg(userVisibleName));
        }
        return;
    }

    std::thread([this, handleId, itemId, outputPath, userVisibleName, openAfterSave]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const auto data = komai::MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
          context, handleId, itemId, 0, 0, false, &error);
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
                  komai::logging::ui()->warn(
                    "Failed to fetch matrix-sdk timeline media '{}' to '{}': {}",
                    userVisibleName.toStdString(),
                    outputPath.toStdString(),
                    error.toStdString());
                  if (mainWindow) {
                      mainWindow->showNotification(
                        tr("Failed to fetch attachment '%1': %2").arg(userVisibleName, error));
                  }
                  return;
              }

              if (openAfterSave) {
                  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath))) {
                      komai::logging::ui()->warn(
                        "Failed to open fetched matrix-sdk timeline media '{}'",
                        outputPath.toStdString());
                      if (mainWindow) {
                          mainWindow->showNotification(
                            tr("Saved attachment '%1' but failed to open it").arg(userVisibleName));
                      }
                  }
                  return;
              }

              if (mainWindow) {
                  const QUrl fileUrl = QUrl::fromLocalFile(outputPath);
                  const QList<komai::NotificationAction> actions{
                    {komai::NotificationAction::OpenUrl,
                     tr("Open"),
                     QStringLiteral("qrc:/icons/icons/ui/open-externally.svg"),
                     fileUrl},
                    {komai::NotificationAction::RevealInFolder,
                     tr("Show in folder"),
                     QStringLiteral("qrc:/icons/icons/ui/folder-open.svg"),
                     fileUrl},
                  };
                  emit mainWindow->showNotificationWithActions(
                    tr("Saved attachment '%1'").arg(userVisibleName),
                    komai::toVariantList(actions));
              }
          },
          Qt::QueuedConnection);
    }).detach();
}
