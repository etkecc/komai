// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <cmath>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <nlohmann/json.hpp>

#include <thread>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "models/ReadReceiptsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/CommunitiesModel.h"
#include "timeline/RoomlistModel.h"
#include "timeline/rawmessage/RawMessageDialogPayload.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "utils/MediaIcons.h"
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
matrixMessageRenderableHtml(const QString &body)
{
    auto html = matrixMessageFormattedHtml(body);
    if (html.isEmpty())
        html = body.toHtmlEscaped().replace(u'\n', QStringLiteral("<br>"));

    html = utils::escapeBlacklistedHtml(html);
    html = utils::linkifyMessage(html);
    return utils::replaceEmoji(html);
}

QString
matrixPendingAttachmentThumbnail(const QString &filePath, const QString &mimeType)
{
    if (!mimeType.startsWith(QStringLiteral("image/"), Qt::CaseInsensitive))
        return {};

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
        return {};

    if (mimeType.compare(QStringLiteral("image/svg+xml"), Qt::CaseInsensitive) == 0)
        return QUrl::fromLocalFile(fileInfo.absoluteFilePath()).toString();

    if (utils::readImageFromFile(fileInfo.absoluteFilePath()).isNull())
        return {};

    return QUrl::fromLocalFile(fileInfo.absoluteFilePath()).toString();
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

bool
isForwardableActiveMatrixTimelineTextKind(const QString &itemKind)
{
    return itemKind == QStringLiteral("message") || itemKind == QStringLiteral("notice") ||
           itemKind == QStringLiteral("emote");
}

bool
isForwardableActiveMatrixTimelineMediaKind(const QString &itemKind)
{
    return itemKind == QStringLiteral("image") || itemKind == QStringLiteral("video") ||
           itemKind == QStringLiteral("audio") || itemKind == QStringLiteral("file");
}

int
estimatedInitialMatrixTimelinePageSize(double viewportHeight)
{
    if (viewportHeight <= 0)
        return 0;

    const auto *settings        = UserSettings::instance().get();
    const auto fontSizePt       = settings ? settings->uiFontSizePt() : 13.0;
    const auto bufferedHeadroom = std::min(viewportHeight * 0.15, fontSizePt * 16.0);
    const auto desiredBufferedHeight = viewportHeight + bufferedHeadroom;
    const auto averageRowHeight      = std::max(56.0, std::round(fontSizePt * 5.25));

    return std::clamp(
      static_cast<int>(std::ceil(desiredBufferedHeight / averageRowHeight)), 15, 40);
}

int
fallbackInitialMatrixTimelinePageSize()
{
    const auto *mainWindow = MainWindow::instance();
    if (!mainWindow)
        return 24;

    const auto *settings       = UserSettings::instance().get();
    const auto fontSizePt      = settings ? settings->uiFontSizePt() : 13.0;
    const auto chromeAllowance = std::max(180.0, std::round(fontSizePt * 14.0));
    const auto approximateViewportHeight =
      std::max(0.0, static_cast<double>(mainWindow->height()) - chromeAllowance);

    return estimatedInitialMatrixTimelinePageSize(approximateViewportHeight);
}

bool
shouldIgnoreMatrixTimelineWarmupShrink(int currentCount, int nextCount)
{
    if (currentCount <= 0 || nextCount >= currentCount)
        return false;

    const auto minimumAcceptedCount =
      std::max(1, static_cast<int>(std::floor(static_cast<double>(currentCount) * 0.8)));
    return nextCount < minimumAcceptedCount;
}
}

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

QString
TimelineViewManager::formatMatrixMessageHtml(const QString &body) const
{
    if (perfUiFlagEnabled(QStringLiteral("disable_timeline_rich_text")))
        return body.toHtmlEscaped().replace(u'\n', QStringLiteral("<br>"));

    return matrixMessageRenderableHtml(body);
}

int
TimelineViewManager::matrixTimelineAttachmentCount() const
{
    return matrixPendingAttachmentItems_.size();
}

void
TimelineViewManager::scheduleCurrentMatrixTimelineSelectionUpdate()
{
    const auto preview = rooms_->currentRoomPreview();
    const auto roomId =
      (!rooms_->currentRoom() && preview.isMatrixSummary()) ? preview.roomid() : QString();

    if (!matrixTimelineSelectionUpdateQueued_ && roomId == activeMatrixTimelineRoomId_)
        return;

    if (!roomId.isEmpty())
        markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_selection_queued");

    if (matrixTimelineSelectionUpdateQueued_)
        return;

    matrixTimelineSelectionUpdateQueued_ = true;
    QMetaObject::invokeMethod(
      this,
      [this]() {
          matrixTimelineSelectionUpdateQueued_ = false;

          const auto preview = rooms_->currentRoomPreview();
          const auto roomId =
            (!rooms_->currentRoom() && preview.isMatrixSummary()) ? preview.roomid() : QString();
          if (!roomId.isEmpty())
              markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_selection_dequeued");

          updateCurrentMatrixTimelineSelection();
      },
      Qt::QueuedConnection);
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

    setPreferredInitialMatrixTimelinePageSize(preferredInitialMatrixTimelinePageSize_ > 0
                                                ? preferredInitialMatrixTimelinePageSize_
                                                : fallbackInitialMatrixTimelinePageSize());

    const auto warmupGeneration = ++matrixTimelineWarmupGuardGeneration_;
    matrixTimelineWarmupGuardActive_ = true;
    QTimer::singleShot(1500, this, [this, roomId, warmupGeneration]() {
        if (matrixTimelineWarmupGuardGeneration_ != warmupGeneration)
            return;
        if (activeMatrixTimelineRoomId_ != roomId)
            return;
        matrixTimelineWarmupGuardActive_ = false;
    });

    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_select_begin");
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
    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_select_done");

    activeMatrixTimelineRoomId_             = roomId;
    matrixTimelineLoading_                  = true;
    matrixTimelineInitialPrefetchAttempted_ = false;
    refreshActiveMatrixTimelinePinnedEventIds();
    refreshActiveMatrixTimelineRedactionPermissions();
    if (matrixTimelineModel_)
        matrixTimelineModel_->clear();
    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_loading_started");
    emit matrixTimelineStateChanged();
}

void
TimelineViewManager::scheduleCurrentMatrixTimelineRefresh()
{
    const auto roomId = activeMatrixTimelineRoomId_;
    if (roomId.isEmpty())
        return;

    if (matrixTimelineRefreshQueued_)
        return;

    matrixTimelineRefreshQueued_ = true;
    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_refresh_queued");
    const auto delayMs =
      (matrixTimelineLoading_ && matrixTimelineModel_ && matrixTimelineModel_->count() == 0) ? 25
                                                                                             : 0;

    QTimer::singleShot(delayMs, this, [this, roomId]() {
        matrixTimelineRefreshQueued_ = false;

        if (!matrixTimelineRefreshPending_ || matrixTimelineRefreshPendingRoomId_ != roomId ||
            activeMatrixTimelineRoomId_ != roomId) {
            return;
        }

        matrixTimelineRefreshPending_ = false;
        markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_refresh_dequeued");
        refreshCurrentMatrixTimeline();
    });
}

bool
TimelineViewManager::refreshActiveMatrixTimelinePinnedEventIds()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

    QStringList pinnedEventIds;
    if (handleId != 0 && !activeMatrixTimelineRoomId_.isEmpty()) {
        QString error;
        const auto pinned = komai::MatrixBackendRuntimeService::fetchRoomPinnedEventIds(
          handleId, activeMatrixTimelineRoomId_, &error);
        if (!pinned) {
            nhlog::ui()->warn("Failed to fetch matrix-sdk room pinned events for '{}' on handle "
                              "{}: {}",
                              activeMatrixTimelineRoomId_.toStdString(),
                              handleId,
                              error.toStdString());
        } else {
            pinnedEventIds = *pinned;
        }
    }

    if (matrixTimelinePinnedEventIds_ == pinnedEventIds)
        return false;

    matrixTimelinePinnedEventIds_ = std::move(pinnedEventIds);
    return true;
}

bool
TimelineViewManager::refreshActiveMatrixTimelineRedactionPermissions()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

    bool canRedactOwn   = false;
    bool canRedactOther = false;

    if (handleId != 0 && !activeMatrixTimelineRoomId_.isEmpty()) {
        QString error;
        const auto permissions = komai::MatrixBackendRuntimeService::fetchRoomRedactionPermissions(
          handleId, activeMatrixTimelineRoomId_, &error);
        if (!permissions) {
            nhlog::ui()->warn("Failed to fetch matrix-sdk room redaction permissions for '{}' on "
                              "handle {}: {}",
                              activeMatrixTimelineRoomId_.toStdString(),
                              handleId,
                              error.toStdString());
        } else {
            canRedactOwn   = permissions->canRedactOwn;
            canRedactOther = permissions->canRedactOther;
        }
    }

    if (matrixTimelineCanRedactOwn_ == canRedactOwn &&
        matrixTimelineCanRedactOther_ == canRedactOther) {
        return false;
    }

    matrixTimelineCanRedactOwn_   = canRedactOwn;
    matrixTimelineCanRedactOther_ = canRedactOther;
    return true;
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

    if (matrixTimelineRefreshInFlightRequestId_ != 0 &&
        matrixTimelineRefreshInFlightRoomId_ == activeMatrixTimelineRoomId_) {
        return;
    }

    const auto roomId                       = activeMatrixTimelineRoomId_;
    const auto requestId                    = ++matrixTimelineRefreshRequestId_;
    matrixTimelineRefreshInFlightRequestId_ = requestId;
    matrixTimelineRefreshInFlightRoomId_    = roomId;

    markRoomSwitchPhaseCpp(activeMatrixTimelineRoomId_, "cpp.matrix_timeline_fetch_begin");

    QPointer<TimelineViewManager> guard(this);
    std::thread([guard, handleId, roomId, requestId]() {
        QString error;
        QElapsedTimer fetchTimer;
        fetchTimer.start();
        const auto items =
          komai::MatrixBackendRuntimeService::fetchActiveRoomTimeline(handleId, &error);
        const auto fetchElapsedUs = fetchTimer.nsecsElapsed() / 1000;

        if (!guard)
            return;

        QMetaObject::invokeMethod(
          guard,
          [guard, handleId, roomId, requestId, items, error, fetchElapsedUs]() mutable {
              if (!guard)
                  return;

              const bool isInFlightRequest =
                guard->matrixTimelineRefreshInFlightRequestId_ == requestId &&
                guard->matrixTimelineRefreshInFlightRoomId_ == roomId;
              if (isInFlightRequest) {
                  guard->matrixTimelineRefreshInFlightRequestId_ = 0;
                  guard->matrixTimelineRefreshInFlightRoomId_.clear();
              }

              auto *mainWindow = MainWindow::instance();
              if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
                  return;

              if (guard->activeMatrixTimelineRoomId_ != roomId) {
                  if (guard->matrixTimelineRefreshPending_ &&
                      guard->matrixTimelineRefreshPendingRoomId_ ==
                        guard->activeMatrixTimelineRoomId_) {
                      guard->scheduleCurrentMatrixTimelineRefresh();
                  }
                  return;
              }

              guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_fetch_done");

              if (guard->roomSwitchPerfEnabled()) {
                  nhlog::ui()->info("[room-switch-perf] "
                                    "phase=cpp.matrix_timeline_fetch_thread_done room='{}' us={}",
                                    roomId.toStdString(),
                                    fetchElapsedUs);
              }

              if (!items) {
                  nhlog::ui()->warn(
                    "Failed to fetch active matrix-sdk room timeline for '{}' on handle {}: {}",
                    roomId.toStdString(),
                    handleId,
                    error.toStdString());
                  guard->clearCurrentMatrixTimeline(false);
                  return;
              }

              const auto preferredInitialPageSize =
                guard->preferredInitialMatrixTimelinePageSize_ > 0
                  ? guard->preferredInitialMatrixTimelinePageSize_
                  : fallbackInitialMatrixTimelinePageSize();
              const auto canDelayFirstPaint = guard->matrixTimelineLoading_ &&
                                              guard->matrixTimelineModel_ &&
                                              guard->matrixTimelineModel_->count() == 0 &&
                                              !guard->matrixTimelineInitialPrefetchAttempted_;
              const auto itemCount = items->size();
              const auto currentModelCount =
                guard->matrixTimelineModel_ ? guard->matrixTimelineModel_->count() : 0;
              if (guard->matrixTimelineWarmupGuardActive_ &&
                  shouldIgnoreMatrixTimelineWarmupShrink(currentModelCount, itemCount)) {
                  if (guard->roomSwitchPerfEnabled()) {
                      nhlog::ui()->info(
                        "[room-switch-perf] phase=cpp.matrix_timeline_warmup_shrink_ignored "
                        "room='{}' current_count={} next_count={}",
                        roomId.toStdString(),
                        currentModelCount,
                        itemCount);
                  }

                  if (guard->matrixTimelineRefreshPending_ &&
                      guard->matrixTimelineRefreshPendingRoomId_ == roomId) {
                      guard->scheduleCurrentMatrixTimelineRefresh();
                  }
                  return;
              }

              if (canDelayFirstPaint && itemCount > 0 && itemCount < preferredInitialPageSize) {
                  const auto shortfall =
                    std::clamp(static_cast<int>(preferredInitialPageSize - itemCount), 1, 24);
                  QString paginateError;
                  if (komai::MatrixBackendRuntimeService::paginateActiveRoomTimelineBackwards(
                        handleId, static_cast<uint16_t>(shortfall), &paginateError)) {
                      guard->matrixTimelineInitialPrefetchAttempted_ = true;
                      if (guard->roomSwitchPerfEnabled()) {
                          nhlog::ui()->info(
                            "[room-switch-perf] phase=cpp.matrix_timeline_initial_prefetch "
                            "room='{}' item_count={} target_count={} request_count={}",
                            roomId.toStdString(),
                            itemCount,
                            preferredInitialPageSize,
                            shortfall);
                      }
                      return;
                  }

                  nhlog::ui()->warn("Failed to prefetch additional matrix-sdk room timeline items "
                                    "for '{}' on handle {}: {}",
                                    roomId.toStdString(),
                                    handleId,
                                    paginateError.toStdString());
                  guard->matrixTimelineInitialPrefetchAttempted_ = true;
              }

              guard->matrixTimelineModel_->replaceItems(*items);
              guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_model_replaced");

              auto stateChanged = false;
              stateChanged = guard->refreshActiveMatrixTimelinePinnedEventIds() || stateChanged;
              stateChanged =
                guard->refreshActiveMatrixTimelineRedactionPermissions() || stateChanged;

              if (guard->matrixTimelineLoading_) {
                  guard->matrixTimelineLoading_ = false;
                  guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_loading_finished");
                  stateChanged = true;
              }

              if (stateChanged)
                  emit guard->matrixTimelineStateChanged();

              guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_snapshot_refreshed");

              if (guard->matrixTimelineRefreshPending_ &&
                  guard->matrixTimelineRefreshPendingRoomId_ == roomId) {
                  guard->scheduleCurrentMatrixTimelineRefresh();
              }
          },
          Qt::QueuedConnection);
    }).detach();
}

void
TimelineViewManager::setPreferredInitialMatrixTimelinePageSize(int pageSize)
{
    const auto clampedPageSize = std::clamp(pageSize, 0, 50);
    if (preferredInitialMatrixTimelinePageSize_ == clampedPageSize)
        return;

    preferredInitialMatrixTimelinePageSize_ = clampedPageSize;

    if (roomSwitchPerfEnabled_) {
        nhlog::ui()->info(
          "[room-switch-perf] phase=cpp.matrix_timeline_initial_page_size_hint page_size={}",
          preferredInitialMatrixTimelinePageSize_);
    }
}

void
TimelineViewManager::clearCurrentMatrixTimeline(bool stopBackendTask)
{
    bool stateChanged = clearActiveMatrixReplyState();
    stateChanged      = clearActiveMatrixEditState() || stateChanged;

    if (!pendingMatrixAttachments_.empty() || !matrixPendingAttachmentItems_.empty()) {
        pendingMatrixAttachments_.clear();
        for (auto *attachment : matrixPendingAttachmentItems_) {
            if (attachment)
                attachment->deleteLater();
        }
        matrixPendingAttachmentItems_.clear();
        stateChanged = true;
    }

    if (matrixTimelineLoading_) {
        matrixTimelineLoading_ = false;
        stateChanged           = true;
    }

    matrixTimelineWarmupGuardActive_ = false;
    ++matrixTimelineWarmupGuardGeneration_;

    if (!matrixTimelinePinnedEventIds_.isEmpty()) {
        matrixTimelinePinnedEventIds_.clear();
        stateChanged = true;
    }

    if (matrixTimelineCanRedactOwn_ || matrixTimelineCanRedactOther_) {
        matrixTimelineCanRedactOwn_   = false;
        matrixTimelineCanRedactOther_ = false;
        stateChanged                  = true;
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

    matrixTimelineRefreshQueued_  = false;
    matrixTimelineRefreshPending_ = false;
    matrixTimelineRefreshPendingRoomId_.clear();
    matrixTimelineRefreshInFlightRequestId_ = 0;
    matrixTimelineRefreshInFlightRoomId_.clear();
    matrixTimelineInitialPrefetchAttempted_ = false;

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

QString
TimelineViewManager::normalizedMatrixMessageKind(const QString &messageKind) const
{
    const auto normalizedKind = messageKind.trimmed().toLower();
    if (normalizedKind == QStringLiteral("notice"))
        return QStringLiteral("notice");
    if (normalizedKind == QStringLiteral("emote"))
        return QStringLiteral("emote");
    return QStringLiteral("text");
}

bool
TimelineViewManager::queueActiveMatrixEdit(const QString &eventId,
                                           const QString &body,
                                           const QString &messageKind)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return false;

    if (matrixAttachmentUploadInFlight_ || !pendingMatrixAttachments_.empty())
        return false;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    if (body.trimmed().isEmpty())
        return false;

    const auto clearedReplyState = clearActiveMatrixReplyState();
    const auto stateChanged =
      setActiveMatrixEditState(trimmedEventId, normalizedMatrixMessageKind(messageKind));

    if (!stateChanged && !clearedReplyState)
        return false;

    if (clearedReplyState)
        emit replyClosed();
    emit matrixTimelineStateChanged();
    return true;
}

void
TimelineViewManager::clearActiveMatrixEdit()
{
    if (clearActiveMatrixEditState())
        emit matrixTimelineStateChanged();
}

bool
TimelineViewManager::sendActiveMatrixEditMessage(const QString &body)
{
    const auto plainBody = body.trimmed();
    if (plainBody.isEmpty())
        return false;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty() ||
        matrixTimelineEditEventId_.trimmed().isEmpty()) {
        nhlog::ui()->warn("Refusing to send matrix-sdk room edit without an active runtime "
                          "handle, selected matrix room, and edit target");
        return false;
    }

    QString error;
    const bool ok =
      komai::MatrixBackendRuntimeService::sendRoomEditMessage(handleId,
                                                              activeMatrixTimelineRoomId_,
                                                              matrixTimelineEditEventId_.trimmed(),
                                                              plainBody,
                                                              matrixMessageFormattedHtml(body),
                                                              matrixTimelineEditMessageKind_,
                                                              &error);

    if (!ok) {
        nhlog::ui()->warn(
          "Failed to queue matrix-sdk room edit for '{}' on handle {} targeting '{}': {}",
          activeMatrixTimelineRoomId_.toStdString(),
          handleId,
          matrixTimelineEditEventId_.toStdString(),
          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to edit message: %1").arg(error));
        return false;
    }

    if (clearActiveMatrixEditState())
        emit matrixTimelineStateChanged();

    return true;
}

bool
TimelineViewManager::queueActiveMatrixReply(const QString &eventId,
                                            const QString &senderId,
                                            const QString &senderDisplayName,
                                            const QString &body)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedEventId  = eventId.trimmed();
    const auto trimmedSenderId = senderId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto trimmedSenderDisplayName = senderDisplayName.trimmed();
    const auto trimmedBody              = body.trimmed();
    const auto changed                  = setActiveMatrixReplyState(
      trimmedEventId, trimmedSenderId, trimmedSenderDisplayName, trimmedBody);
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
TimelineViewManager::toggleActiveMatrixTimelineReaction(const QString &eventId,
                                                        const QString &reactionKey)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to toggle a matrix-sdk room reaction without an active "
                          "runtime handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId     = eventId.trimmed();
    const auto trimmedReactionKey = reactionKey.trimmed();
    if (trimmedEventId.isEmpty() || trimmedReactionKey.isEmpty())
        return false;

    QString error;
    if (!komai::MatrixBackendRuntimeService::toggleRoomReaction(
          handleId, activeMatrixTimelineRoomId_, trimmedEventId, trimmedReactionKey, &error)) {
        nhlog::ui()->warn("Failed to toggle matrix-sdk room reaction for '{}' on handle {}: {}",
                          activeMatrixTimelineRoomId_.toStdString(),
                          handleId,
                          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to react: %1").arg(error));
        return false;
    }

    return true;
}

bool
TimelineViewManager::redactActiveMatrixTimelineEvent(const QString &eventId, const QString &reason)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to redact a matrix-sdk room event without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    QString error;
    if (!komai::MatrixBackendRuntimeService::redactRoomEvent(
          handleId, activeMatrixTimelineRoomId_, trimmedEventId, reason.trimmed(), &error)) {
        nhlog::ui()->warn("Failed to redact matrix-sdk room event '{}' in '{}' on handle {}: {}",
                          trimmedEventId.toStdString(),
                          activeMatrixTimelineRoomId_.toStdString(),
                          handleId,
                          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to delete message: %1").arg(error));
        return false;
    }

    if (matrixTimelineModel_)
        matrixTimelineModel_->redactItemByEventId(trimmedEventId);

    return true;
}

bool
TimelineViewManager::markActiveMatrixTimelineEventAsRead(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to mark a matrix-sdk room event as read without an active "
                          "runtime handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    QString error;
    if (!komai::MatrixBackendRuntimeService::markRoomEventAsRead(
          handleId, activeMatrixTimelineRoomId_, trimmedEventId, &error)) {
        nhlog::ui()->warn(
          "Failed to mark matrix-sdk room event '{}' as read in '{}' on handle {}: {}",
          trimmedEventId.toStdString(),
          activeMatrixTimelineRoomId_.toStdString(),
          handleId,
          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to mark message as read: %1").arg(error));
        return false;
    }

    return true;
}

bool
TimelineViewManager::reportActiveMatrixTimelineEvent(const QString &eventId,
                                                     const QString &reason,
                                                     int score)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to report a matrix-sdk room event without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    QString error;
    if (!komai::MatrixBackendRuntimeService::reportRoomEvent(
          handleId, activeMatrixTimelineRoomId_, trimmedEventId, reason, score, &error)) {
        nhlog::ui()->warn("Failed to report matrix-sdk room event '{}' in '{}' on handle {}: {}",
                          trimmedEventId.toStdString(),
                          activeMatrixTimelineRoomId_.toStdString(),
                          handleId,
                          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to report message: %1").arg(error));
        return false;
    }

    return true;
}

bool
TimelineViewManager::forwardActiveMatrixTimelineEvent(const QString &eventId,
                                                      const QString &targetRoomId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to forward a matrix-sdk room event without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId      = eventId.trimmed();
    const auto trimmedTargetRoomId = targetRoomId.trimmed();
    if (trimmedEventId.isEmpty() || trimmedTargetRoomId.isEmpty())
        return false;

    if (!matrixTimelineModel_) {
        nhlog::ui()->warn("Refusing to forward matrix-sdk room event '{}' without an active "
                          "timeline model",
                          trimmedEventId.toStdString());
        return false;
    }

    const auto item = matrixTimelineModel_->itemByEventId(trimmedEventId);
    if (!item) {
        nhlog::ui()->warn("Refusing to forward unknown matrix-sdk room event '{}' in '{}'",
                          trimmedEventId.toStdString(),
                          activeMatrixTimelineRoomId_.toStdString());
        return false;
    }

    const auto itemKind = item->itemKind.trimmed().toLower();
    QString error;

    if (isForwardableActiveMatrixTimelineTextKind(itemKind)) {
        const auto ok = komai::MatrixBackendRuntimeService::sendRoomMessage(
          handleId,
          trimmedTargetRoomId,
          item->body,
          matrixMessageFormattedHtml(item->body),
          normalizedMatrixMessageKind(itemKind),
          &error);
        if (!ok) {
            nhlog::ui()->warn("Failed to forward matrix-sdk room event '{}' from '{}' to '{}' on "
                              "handle {}: {}",
                              trimmedEventId.toStdString(),
                              activeMatrixTimelineRoomId_.toStdString(),
                              trimmedTargetRoomId.toStdString(),
                              handleId,
                              error.toStdString());
            if (mainWindow) {
                mainWindow->showNotification(tr("Failed to forward message: %1").arg(error));
            }
            return false;
        }

        return true;
    }

    if (!isForwardableActiveMatrixTimelineMediaKind(itemKind)) {
        nhlog::ui()->warn("Forwarding matrix-sdk room event kind '{}' is not implemented yet",
                          itemKind.toStdString());
        if (mainWindow) {
            mainWindow->showNotification(tr("Forwarding this message type is not available yet."));
        }
        return false;
    }

    const auto sourceItemId      = item->itemId.trimmed().isEmpty() ? trimmedEventId : item->itemId;
    const auto suggestedFileName = matrixTimelineAttachmentFileName(item->fileName, sourceItemId);
    const auto mimeType          = item->mimeType.trimmed().isEmpty()
                                     ? QStringLiteral("application/octet-stream")
                                     : item->mimeType.trimmed();
    const auto suffix            = QFileInfo(suggestedFileName).suffix().trimmed();
    auto tempFileName =
      QStringLiteral("forward-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    if (!suffix.isEmpty())
        tempFileName += QStringLiteral(".") + suffix;
    const auto outputPath = matrixTimelineMediaCachePath(tempFileName);
    const auto caption    = item->body;

    std::thread([handleId,
                 sourceItemId,
                 sourceRoomId = activeMatrixTimelineRoomId_,
                 targetRoomId = trimmedTargetRoomId,
                 outputPath,
                 suggestedFileName,
                 mimeType,
                 caption]() {
        QString error;
        const auto data = komai::MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
          handleId, sourceItemId, 0, 0, false, &error);
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
            }
        }

        if (ok) {
            ok = komai::MatrixBackendRuntimeService::sendRoomAttachment(
              handleId, targetRoomId, outputPath, suggestedFileName, caption, {}, mimeType, &error);
        }

        QFile::remove(outputPath);

        auto *callbackContext = QCoreApplication::instance();
        if (!callbackContext)
            return;

        QMetaObject::invokeMethod(
          callbackContext,
          [handleId, ok, sourceItemId, sourceRoomId, targetRoomId, suggestedFileName, error]() {
              if (ok)
                  return;

              nhlog::ui()->warn("Failed to forward matrix-sdk room media '{}' from '{}' to '{}' "
                                "on handle {}: {}",
                                sourceItemId.toStdString(),
                                sourceRoomId.toStdString(),
                                targetRoomId.toStdString(),
                                handleId,
                                error.toStdString());
              if (auto *mainWindow = MainWindow::instance()) {
                  mainWindow->showNotification(
                    TimelineViewManager::tr("Failed to forward attachment '%1': %2")
                      .arg(suggestedFileName, error));
              }
          },
          Qt::QueuedConnection);
    }).detach();

    return true;
}

bool
TimelineViewManager::pinActiveMatrixTimelineEvent(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to pin a matrix-sdk room event without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    QString error;
    if (!komai::MatrixBackendRuntimeService::pinRoomEvent(
          handleId, activeMatrixTimelineRoomId_, trimmedEventId, &error)) {
        nhlog::ui()->warn("Failed to pin matrix-sdk room event '{}' in '{}' on handle {}: {}",
                          trimmedEventId.toStdString(),
                          activeMatrixTimelineRoomId_.toStdString(),
                          handleId,
                          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to pin message: %1").arg(error));
        return false;
    }

    if (!matrixTimelinePinnedEventIds_.contains(trimmedEventId)) {
        matrixTimelinePinnedEventIds_.push_back(trimmedEventId);
        emit matrixTimelineStateChanged();
    }

    return true;
}

bool
TimelineViewManager::unpinActiveMatrixTimelineEvent(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to unpin a matrix-sdk room event without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    QString error;
    if (!komai::MatrixBackendRuntimeService::unpinRoomEvent(
          handleId, activeMatrixTimelineRoomId_, trimmedEventId, &error)) {
        nhlog::ui()->warn("Failed to unpin matrix-sdk room event '{}' in '{}' on handle {}: {}",
                          trimmedEventId.toStdString(),
                          activeMatrixTimelineRoomId_.toStdString(),
                          handleId,
                          error.toStdString());
        if (mainWindow)
            mainWindow->showNotification(tr("Failed to unpin message: %1").arg(error));
        return false;
    }

    if (matrixTimelinePinnedEventIds_.removeAll(trimmedEventId) > 0)
        emit matrixTimelineStateChanged();

    return true;
}

QVariantMap
TimelineViewManager::rawMessageDialogForActiveMatrixTimelineEvent(const QString &eventId) const
{
    QVariantMap dialogData;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty())
        return dialogData;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return dialogData;

    QString error;
    const auto rawEventJson = komai::MatrixBackendRuntimeService::fetchActiveRoomRawEventJson(
      handleId, activeMatrixTimelineRoomId_, trimmedEventId, &error);
    if (!rawEventJson) {
        nhlog::ui()->warn(
          "Failed to fetch raw JSON for matrix-sdk room event '{}' in '{}' on handle {}: {}",
          trimmedEventId.toStdString(),
          activeMatrixTimelineRoomId_.toStdString(),
          handleId,
          error.toStdString());
        return dialogData;
    }

    try {
        const auto eventJson = nlohmann::json::parse(rawEventJson->toStdString());
        const auto timelinePalette =
          Theme::paletteFromTheme(UserSettings::instance()->uiThemeSlug());
        const auto dialogPayload =
          timeline::rawmessage::buildRawMessageDialogPayload(eventJson, timelinePalette);
        dialogData.insert(QStringLiteral("renderedRawMessage"), dialogPayload.renderedRawMessage);
        dialogData.insert(QStringLiteral("rawMessageJson"), dialogPayload.rawMessageJson);
        dialogData.insert(QStringLiteral("rawMessageBody"), dialogPayload.rawMessageBody);
        dialogData.insert(QStringLiteral("rawMessageFormattedBody"),
                          dialogPayload.rawMessageFormattedBody);
    } catch (const std::exception &e) {
        nhlog::ui()->warn("Failed to parse raw JSON for matrix-sdk room event '{}' in '{}': {}",
                          trimmedEventId.toStdString(),
                          activeMatrixTimelineRoomId_.toStdString(),
                          e.what());
    }

    return dialogData;
}

QObject *
TimelineViewManager::readReceiptsModelForActiveMatrixTimelineEvent(const QString &eventId) const
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty())
        return nullptr;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return nullptr;

    QString error;
    const auto receipts = komai::MatrixBackendRuntimeService::fetchRoomReadReceipts(
      handleId, activeMatrixTimelineRoomId_, trimmedEventId, &error);
    if (!receipts) {
        nhlog::ui()->warn(
          "Failed to fetch matrix-sdk room read receipts for event '{}' in '{}' on handle {}: {}",
          trimmedEventId.toStdString(),
          activeMatrixTimelineRoomId_.toStdString(),
          handleId,
          error.toStdString());
        return nullptr;
    }

    QVector<ReadReceiptEntry> entries;
    entries.reserve(receipts->size());
    for (const auto &entry : *receipts) {
        entries.push_back(ReadReceiptEntry{
          .mxid         = entry.userId,
          .displayName  = entry.displayName,
          .avatarUrl    = entry.avatarUrl,
          .rawTimestamp = entry.timestamp == 0
                            ? QDateTime{}
                            : QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(entry.timestamp)),
        });
    }

    auto *model = new ReadReceiptsProxy{std::move(entries), activeMatrixTimelineRoomId_};
    QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
    return model;
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

    if (!matrixTimelineEditEventId_.isEmpty()) {
        nhlog::ui()->warn(
          "Refusing to stage matrix-sdk room attachments while editing an existing message");
        if (mainWindow) {
            mainWindow->showNotification(
              tr("Finish editing the current message before attaching files."));
        }
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
        const auto effectiveMimeType =
          mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType;
        const auto fileName = QFileInfo(filePath).fileName();
        pendingMatrixAttachments_.push_back(PendingMatrixAttachment{
          .handleId     = handleId,
          .roomId       = activeMatrixTimelineRoomId_,
          .filePath     = filePath,
          .filename     = fileName,
          .body         = {},
          .replyEventId = {},
          .mimeType     = effectiveMimeType,
        });
        matrixPendingAttachmentItems_.push_back(new MatrixPendingAttachmentUpload(
          filePath,
          fileName,
          effectiveMimeType,
          utils::fileTypeIconSource(effectiveMimeType),
          matrixPendingAttachmentThumbnail(filePath, effectiveMimeType),
          this));
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
    if (!replyEventId.isEmpty()) {
        for (auto &attachment : pendingMatrixAttachments_) {
            if (attachment.replyEventId.isEmpty())
                attachment.replyEventId = replyEventId;
        }
    }

    const auto clearedReplyState = clearActiveMatrixReplyState();
    startNextPendingMatrixAttachment();
    if (clearedReplyState)
        emit replyClosed();
    return true;
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

void
TimelineViewManager::handleMatrixBackendRoomListSnapshotUpdated(std::uint64_t handleId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    rooms_->refreshMatrixBackendRooms();
    scheduleMatrixSidebarRefresh();

    if (waitingForFirstSync_) {
        nhlog::ui()->info("Clearing waitingForFirstSync from first matrix-sdk room-list snapshot "
                          "for handle {}",
                          handleId);
        waitingForFirstSync_ = false;
        emit waitingForFirstSyncChanged(false);
    }
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

    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_snapshot_signal");

    if (waitingForFirstSync_) {
        nhlog::ui()->info("Clearing waitingForFirstSync from first active matrix-sdk room "
                          "timeline snapshot for handle {} room '{}'",
                          handleId,
                          roomId.toStdString());
        waitingForFirstSync_ = false;
        emit waitingForFirstSyncChanged(false);
    }

    matrixTimelineRefreshPending_       = true;
    matrixTimelineRefreshPendingRoomId_ = roomId;

    scheduleCurrentMatrixTimelineRefresh();
}

bool
TimelineViewManager::paginateActiveMatrixTimelineBackwards(int pageSize)
{
    if (matrixTimelineModel_ &&
        matrixTimelineModel_->revealOlderItems(pageSize > 0 ? pageSize : 50)) {
        return true;
    }

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

    auto attachment = pendingMatrixAttachments_.front();
    if (!matrixPendingAttachmentItems_.isEmpty() && matrixPendingAttachmentItems_.front()) {
        const auto *item    = matrixPendingAttachmentItems_.front();
        attachment.filename = item->filename().trimmed();
        attachment.body     = item->body().trimmed();
    }
    matrixAttachmentUploadInFlight_ = true;
    emit matrixTimelineStateChanged();

    std::thread([this, attachment]() {
        QString error;
        const bool ok =
          komai::MatrixBackendRuntimeService::sendRoomAttachment(attachment.handleId,
                                                                 attachment.roomId,
                                                                 attachment.filePath,
                                                                 attachment.filename,
                                                                 attachment.body,
                                                                 attachment.replyEventId,
                                                                 attachment.mimeType,
                                                                 &error);

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
                                               const QString &senderId,
                                               const QString &senderDisplayName,
                                               const QString &body)
{
    const auto trimmedEventId           = eventId.trimmed();
    const auto trimmedSenderId          = senderId.trimmed();
    const auto trimmedSenderDisplayName = senderDisplayName.trimmed();
    const auto trimmedBody              = body.trimmed();

    if (matrixTimelineReplyEventId_ == trimmedEventId &&
        matrixTimelineReplySenderId_ == trimmedSenderId &&
        matrixTimelineReplySenderDisplayName_ == trimmedSenderDisplayName &&
        matrixTimelineReplyBody_ == trimmedBody) {
        return false;
    }

    matrixTimelineReplyEventId_           = trimmedEventId;
    matrixTimelineReplySenderId_          = trimmedSenderId;
    matrixTimelineReplySenderDisplayName_ = trimmedSenderDisplayName;
    matrixTimelineReplyBody_              = trimmedBody;
    emit replyingEventChanged(matrixTimelineReplyEventId_);
    return true;
}

bool
TimelineViewManager::clearActiveMatrixReplyState()
{
    if (matrixTimelineReplyEventId_.isEmpty() && matrixTimelineReplySenderId_.isEmpty() &&
        matrixTimelineReplySenderDisplayName_.isEmpty() && matrixTimelineReplyBody_.isEmpty()) {
        return false;
    }

    matrixTimelineReplyEventId_.clear();
    matrixTimelineReplySenderId_.clear();
    matrixTimelineReplySenderDisplayName_.clear();
    matrixTimelineReplyBody_.clear();
    emit replyingEventChanged(QString());
    return true;
}

bool
TimelineViewManager::setActiveMatrixEditState(const QString &eventId, const QString &messageKind)
{
    const auto trimmedEventId    = eventId.trimmed();
    const auto normalizedMessage = normalizedMatrixMessageKind(messageKind);

    if (matrixTimelineEditEventId_ == trimmedEventId &&
        matrixTimelineEditMessageKind_ == normalizedMessage) {
        return false;
    }

    matrixTimelineEditEventId_     = trimmedEventId;
    matrixTimelineEditMessageKind_ = normalizedMessage;
    return true;
}

bool
TimelineViewManager::clearActiveMatrixEditState()
{
    if (matrixTimelineEditEventId_.isEmpty() && matrixTimelineEditMessageKind_.isEmpty())
        return false;

    matrixTimelineEditEventId_.clear();
    matrixTimelineEditMessageKind_.clear();
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
    if (!matrixPendingAttachmentItems_.isEmpty()) {
        auto *pendingItem = matrixPendingAttachmentItems_.takeFirst();
        if (pendingItem)
            pendingItem->deleteLater();
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
