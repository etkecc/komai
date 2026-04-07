// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <cmath>

#include <QBuffer>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
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

#include <thread>

#include "chat/ChatPage.h"
#include "emoji/EmoticonReplace.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "models/ReadReceiptsModel.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/CommunitiesModel.h"
#include "timeline/RoomlistModel.h"
#include "timeline/formattedcode/RawJsonFormatter.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "utils/MediaIcons.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

namespace {
bool
matrixMessageUsesMarkdownFormatting()
{
    auto *chatPage       = ChatPage::instance();
    const auto *settings = chatPage ? chatPage->userSettings().get() : nullptr;
    return settings && settings->composerInputMarkdownToHtmlEnabled();
}

QString
renderPlainMatrixMessageHtml(const QString &body)
{
    auto html = body.toHtmlEscaped().replace(u'\n', QStringLiteral("<br>"));
    html      = utils::escapeBlacklistedHtml(html);
    html      = utils::linkifyMessage(html);
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

int
estimatedInitialMatrixTimelinePageSize(double viewportHeight)
{
    if (viewportHeight <= 0)
        return 0;

    const auto *settings             = UserSettings::instance().get();
    const auto fontSizePt            = settings ? settings->uiFontSizePt() : 13.0;
    const auto bufferedHeadroom      = std::min(viewportHeight * 0.15, fontSizePt * 16.0);
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

struct MatrixTimelineRoomStateSnapshot
{
    QStringList pinnedEventIds;
    QStringList frequentReactions;
    bool fetchedFrequentReactions       = false;
    bool canCacheEmptyFrequentReactions = false;
    bool canRedactOwn                   = false;
    bool canRedactOther                 = false;
};

struct MatrixTimelineEventActionResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString eventId;
    QString detail;
    QString error;
    bool ok = false;
};

struct MatrixTimelineMessageSendResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString targetEventId;
    QString action;
    QString error;
    bool ok = false;
};

struct MatrixTimelineRawMessageFetchResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString eventId;
    QString prettyJson;
    QString body;
    QString formattedBody;
    QString error;
    bool ok = false;
};

struct MatrixTimelineReadReceiptsFetchResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString eventId;
    QVector<komai::MatrixReadReceiptEntry> receipts;
    QString error;
    bool ok = false;
};
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

void
TimelineViewManager::primeCurrentMatrixTimelineSelection()
{
    // Run the room-to-timeline handoff immediately when the room summary is
    // selected, instead of waiting for currentRoomChanged fanout through QML
    // and other listeners before we even start the active Rust timeline.
    scheduleCurrentMatrixTimelineSelectionUpdate();
}

QString
TimelineViewManager::formatMatrixMessageHtml(const QString &body) const
{
    if (perfUiFlagEnabled(QStringLiteral("disable_timeline_rich_text")))
        return body.toHtmlEscaped().replace(u'\n', QStringLiteral("<br>"));

    return renderPlainMatrixMessageHtml(body);
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
    const auto roomId  = preview.isMatrixSummary() ? preview.roomid() : QString();

    if (!matrixTimelineSelectionUpdateQueued_ && roomId == activeMatrixTimelineRoomId_)
        return;

    if (!roomId.isEmpty())
        markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_selection_queued");

    if (matrixTimelineSelectionUpdateQueued_)
        return;

    matrixTimelineSelectionUpdateQueued_ = true;
    const auto currentPreview            = rooms_->currentRoomPreview();
    const auto currentRoomId =
      currentPreview.isMatrixSummary() ? currentPreview.roomid() : QString();
    if (!currentRoomId.isEmpty())
        markRoomSwitchPhaseCpp(currentRoomId, "cpp.matrix_timeline_selection_dequeued");

    // Selecting the active Rust timeline now just kicks off a background task,
    // so delaying it behind later event-loop work only adds avoidable room-open
    // latency. Run it immediately while the room-selection state is already hot.
    updateCurrentMatrixTimelineSelection();
    matrixTimelineSelectionUpdateQueued_ = false;
}

void
TimelineViewManager::updateCurrentMatrixTimelineSelection()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

    const auto preview = rooms_->currentRoomPreview();
    if (!preview.isMatrixSummary() || handleId == 0) {
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
    clearActiveMatrixThreadState();

    setPreferredInitialMatrixTimelinePageSize(preferredInitialMatrixTimelinePageSize_ > 0
                                                ? preferredInitialMatrixTimelinePageSize_
                                                : fallbackInitialMatrixTimelinePageSize());

    const auto warmupGeneration      = ++matrixTimelineWarmupGuardGeneration_;
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
    {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        if (const auto it = matrixTimelineFrequentReactionsCache_.constFind(roomId);
            it != matrixTimelineFrequentReactionsCache_.constEnd() &&
            (now - it->timestampMs) <
              settings::core::definitions::kReactionFrequencyCacheDurationMs) {
            matrixTimelineFrequentReactions_ = it->reactions;
        } else {
            matrixTimelineFrequentReactions_.clear();
        }
    }
    refreshActiveMatrixTimelineRoomStateAsync();
    if (matrixTimelineModel_) {
        matrixTimelineModel_->clear();
        matrixTimelineModel_->setRoomId(roomId);
    }
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
    QTimer::singleShot(0, this, [this, roomId]() {
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

void
TimelineViewManager::refreshActiveMatrixTimelineRoomStateAsync()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    const auto roomId   = activeMatrixTimelineRoomId_;

    if (handleId == 0 || roomId.isEmpty()) {
        if (applyActiveMatrixTimelineRoomState({}, {}, false, false))
            emit matrixTimelineStateChanged();
        return;
    }

    const auto now = QDateTime::currentMSecsSinceEpoch();
    QStringList cachedFrequentReactions;
    bool shouldFetchFrequentReactions = true;
    if (const auto it = matrixTimelineFrequentReactionsCache_.constFind(roomId);
        it != matrixTimelineFrequentReactionsCache_.constEnd() &&
        (now - it->timestampMs) < settings::core::definitions::kReactionFrequencyCacheDurationMs) {
        cachedFrequentReactions      = it->reactions;
        shouldFetchFrequentReactions = false;
    }
    const bool canCacheEmptyFrequentReactions =
      matrixTimelineModel_ && matrixTimelineModel_->count() > 0;

    matrixTimelineRoomStateRefreshPending_       = true;
    matrixTimelineRoomStateRefreshPendingRoomId_ = roomId;

    if (matrixTimelineRoomStateInFlightRequestId_ != 0 &&
        matrixTimelineRoomStateInFlightRoomId_ == roomId) {
        return;
    }

    matrixTimelineRoomStateRefreshPending_ = false;
    matrixTimelineRoomStateRefreshPendingRoomId_.clear();

    const auto requestId                      = ++matrixTimelineRoomStateRequestId_;
    matrixTimelineRoomStateInFlightRequestId_ = requestId;
    matrixTimelineRoomStateInFlightRoomId_    = roomId;

    QPointer<TimelineViewManager> guard(this);
    std::thread([guard,
                 handleId,
                 roomId,
                 requestId,
                 cachedFrequentReactions = std::move(cachedFrequentReactions),
                 shouldFetchFrequentReactions,
                 canCacheEmptyFrequentReactions]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        MatrixTimelineRoomStateSnapshot snapshot;
        QString pinnedError;
        QString frequentReactionsError;
        QString permissionsError;

        const auto pinned = komai::MatrixBackendRuntimeService::fetchRoomPinnedEventIds(
          context, handleId, roomId, &pinnedError);
        if (pinned)
            snapshot.pinnedEventIds = *pinned;
        snapshot.frequentReactions              = cachedFrequentReactions;
        snapshot.canCacheEmptyFrequentReactions = canCacheEmptyFrequentReactions;

        if (shouldFetchFrequentReactions) {
            const auto frequentReactions =
              komai::MatrixBackendRuntimeService::fetchRoomFrequentReactions(
                context,
                handleId,
                roomId,
                settings::core::definitions::kReactionFrequencyLookbackDays,
                settings::core::definitions::kMaxQuickReactionSlots,
                settings::core::definitions::kMaxReactionScanEvents,
                &frequentReactionsError);
            if (frequentReactions) {
                snapshot.frequentReactions        = *frequentReactions;
                snapshot.fetchedFrequentReactions = true;
            }
        }

        const auto permissions = komai::MatrixBackendRuntimeService::fetchRoomRedactionPermissions(
          context, handleId, roomId, &permissionsError);
        if (permissions) {
            snapshot.canRedactOwn   = permissions->canRedactOwn;
            snapshot.canRedactOther = permissions->canRedactOther;
        }

        if (!guard)
            return;

        QMetaObject::invokeMethod(
          guard,
          [guard,
           handleId,
           roomId,
           requestId,
           snapshot               = std::move(snapshot),
           pinnedError            = std::move(pinnedError),
           frequentReactionsError = std::move(frequentReactionsError),
           permissionsError       = std::move(permissionsError)]() mutable {
              if (!guard)
                  return;

              const bool isInFlightRequest =
                guard->matrixTimelineRoomStateInFlightRequestId_ == requestId &&
                guard->matrixTimelineRoomStateInFlightRoomId_ == roomId;
              if (isInFlightRequest) {
                  guard->matrixTimelineRoomStateInFlightRequestId_ = 0;
                  guard->matrixTimelineRoomStateInFlightRoomId_.clear();
              }

              auto *mainWindow = MainWindow::instance();
              if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
                  return;

              if (guard->activeMatrixTimelineRoomId_ != roomId) {
                  if (guard->matrixTimelineRoomStateRefreshPending_ &&
                      guard->matrixTimelineRoomStateRefreshPendingRoomId_ ==
                        guard->activeMatrixTimelineRoomId_) {
                      guard->refreshActiveMatrixTimelineRoomStateAsync();
                  }
                  return;
              }

              if (!pinnedError.isEmpty()) {
                  nhlog::ui()->warn("Failed to fetch matrix-sdk room pinned events for '{}' on "
                                    "handle {}: {}",
                                    roomId.toStdString(),
                                    handleId,
                                    pinnedError.toStdString());
              }

              if (!frequentReactionsError.isEmpty()) {
                  nhlog::ui()->warn("Failed to fetch matrix-sdk room frequent reactions for '{}' "
                                    "on handle {}: {}",
                                    roomId.toStdString(),
                                    handleId,
                                    frequentReactionsError.toStdString());
              }

              if (!permissionsError.isEmpty()) {
                  nhlog::ui()->warn("Failed to fetch matrix-sdk room redaction permissions for "
                                    "'{}' on handle {}: {}",
                                    roomId.toStdString(),
                                    handleId,
                                    permissionsError.toStdString());
              }

              if (snapshot.fetchedFrequentReactions && (!snapshot.frequentReactions.isEmpty() ||
                                                        snapshot.canCacheEmptyFrequentReactions)) {
                  guard->matrixTimelineFrequentReactionsCache_.insert(
                    roomId,
                    TimelineViewManager::MatrixTimelineFrequentReactionsCacheEntry{
                      .reactions   = snapshot.frequentReactions,
                      .timestampMs = QDateTime::currentMSecsSinceEpoch(),
                    });
              }

              if (guard->applyActiveMatrixTimelineRoomState(std::move(snapshot.pinnedEventIds),
                                                            std::move(snapshot.frequentReactions),
                                                            snapshot.canRedactOwn,
                                                            snapshot.canRedactOther)) {
                  emit guard->matrixTimelineStateChanged();
              }

              if (guard->matrixTimelineRoomStateRefreshPending_ &&
                  guard->matrixTimelineRoomStateRefreshPendingRoomId_ == roomId) {
                  guard->refreshActiveMatrixTimelineRoomStateAsync();
              }
          },
          Qt::QueuedConnection);
    }).detach();
}

bool
TimelineViewManager::applyActiveMatrixTimelineRoomState(QStringList pinnedEventIds,
                                                        QStringList frequentReactions,
                                                        bool canRedactOwn,
                                                        bool canRedactOther)
{
    if (matrixTimelinePinnedEventIds_ == pinnedEventIds &&
        matrixTimelineFrequentReactions_ == frequentReactions &&
        matrixTimelineCanRedactOwn_ == canRedactOwn &&
        matrixTimelineCanRedactOther_ == canRedactOther) {
        return false;
    }

    matrixTimelinePinnedEventIds_    = std::move(pinnedEventIds);
    matrixTimelineFrequentReactions_ = std::move(frequentReactions);
    matrixTimelineCanRedactOwn_      = canRedactOwn;
    matrixTimelineCanRedactOther_    = canRedactOther;
    return true;
}

void
TimelineViewManager::invalidateMatrixTimelineFrequentReactionsCache(const QString &roomId)
{
    const auto trimmedRoomId = roomId.trimmed();
    if (trimmedRoomId.isEmpty())
        return;

    matrixTimelineFrequentReactionsCache_.remove(trimmedRoomId);
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
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        QElapsedTimer fetchTimer;
        fetchTimer.start();
        const auto items =
          komai::MatrixBackendRuntimeService::fetchActiveRoomTimeline(context, handleId, &error);
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

                      // Schedule a fallback refresh in case the pagination never
                      // produces a new snapshot (e.g. server-side auth failure).
                      // If a real snapshot arrives first, the pending flag will
                      // already be set and this becomes a no-op.
                      QTimer::singleShot(500, guard, [guard, roomId]() {
                          if (!guard)
                              return;
                          if (guard->activeMatrixTimelineRoomId_ != roomId)
                              return;
                          if (guard->matrixTimelineModel_ &&
                              guard->matrixTimelineModel_->count() > 0)
                              return;

                          nhlog::ui()->warn(
                            "Initial prefetch fallback: forcing refresh for room '{}' "
                            "because the model is still empty",
                            roomId.toStdString());

                          guard->matrixTimelineRefreshPending_       = true;
                          guard->matrixTimelineRefreshPendingRoomId_ = roomId;
                          guard->scheduleCurrentMatrixTimelineRefresh();
                      });
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
              guard->refreshActiveMatrixTimelineRoomStateAsync();

              if (guard->matrixTimelineLoading_) {
                  const bool hasContent =
                    itemCount > 0 ||
                    (guard->matrixTimelineModel_ && guard->matrixTimelineModel_->count() > 0);
                  if (hasContent) {
                      guard->matrixTimelineLoading_ = false;
                      guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_loading_finished");
                      stateChanged = true;
                  } else {
                      // Empty snapshot while loading — backward pagination
                      // is still in progress.  Keep showing "Loading this
                      // room…" instead of "Nothing has loaded for this room
                      // yet."  A fallback clears the flag for genuinely empty
                      // rooms (no messages ever).
                      QTimer::singleShot(10000, guard, [guard, roomId]() {
                          if (!guard || guard->activeMatrixTimelineRoomId_ != roomId)
                              return;
                          if (!guard->matrixTimelineLoading_)
                              return;
                          guard->matrixTimelineLoading_ = false;
                          emit guard->matrixTimelineStateChanged();
                      });
                  }
              }

              if (stateChanged)
                  emit guard->matrixTimelineStateChanged();

              guard->rooms_->flushDeferredCurrentRoomVisualState(roomId);
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
    const bool changed         = preferredInitialMatrixTimelinePageSize_ != clampedPageSize;

    preferredInitialMatrixTimelinePageSize_ = clampedPageSize;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId != 0 && clampedPageSize > 0) {
        QString error;
        if (!komai::MatrixBackendRuntimeService::setActiveRoomTimelineInitialPageSize(
              handleId, static_cast<uint16_t>(clampedPageSize), &error)) {
            nhlog::ui()->warn("Failed to update active matrix-sdk room timeline initial page size "
                              "on handle {}: {}",
                              handleId,
                              error.toStdString());
        }
    }

    if (!changed)
        return;

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
    stateChanged |= clearActiveMatrixThreadState();
    stateChanged |= clearActiveMatrixEditState();
    if (!matrixTimelinePendingJumpRoomId_.isEmpty() ||
        !matrixTimelinePendingJumpEventId_.isEmpty() ||
        matrixTimelinePendingJumpPaginationAttempts_ != 0 ||
        matrixTimelinePendingJumpAwaitingSnapshot_ || matrixTimelinePendingJumpExhaustedLogged_) {
        matrixTimelinePendingJumpRoomId_.clear();
        matrixTimelinePendingJumpEventId_.clear();
        matrixTimelinePendingJumpPaginationAttempts_ = 0;
        matrixTimelinePendingJumpAwaitingSnapshot_   = false;
        matrixTimelinePendingJumpExhaustedLogged_    = false;
        stateChanged                                 = true;
    }

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

    if (!matrixTimelineFrequentReactions_.isEmpty()) {
        matrixTimelineFrequentReactions_.clear();
        stateChanged = true;
    }

    if (matrixTimelineCanRedactOwn_ || matrixTimelineCanRedactOther_) {
        matrixTimelineCanRedactOwn_   = false;
        matrixTimelineCanRedactOther_ = false;
        stateChanged                  = true;
    }

    if (!matrixTimelineTypingUsers_.isEmpty()) {
        matrixTimelineTypingUsers_.clear();
        emit matrixTimelineTypingUsersChanged();
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
    matrixTimelineRoomStateRefreshPending_ = false;
    matrixTimelineRoomStateRefreshPendingRoomId_.clear();
    ++matrixTimelineRoomStateRequestId_;
    matrixTimelineRoomStateInFlightRequestId_ = 0;
    matrixTimelineRoomStateInFlightRoomId_.clear();
    clearMatrixReadMarkerQueue();
    matrixTimelineInitialPrefetchAttempted_ = false;

    if (matrixTimelineModel_) {
        matrixTimelineModel_->clear();
        matrixTimelineModel_->setRoomId(QString());
    }

    if (stateChanged)
        emit matrixTimelineStateChanged();
}

bool
TimelineViewManager::sendActiveMatrixTextMessage(const QString &body)
{
    const auto plainBody = emoji::replaceEmoticons(
      body.trimmed(), UserSettings::instance()->composerInputAutoReplaceEmoji());
    if (plainBody.isEmpty())
        return false;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to send matrix-sdk room message without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto roomId                = activeMatrixTimelineRoomId_;
    const auto useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();
    const auto replyEventId          = matrixTimelineReplyEventId_.trimmed();
    const auto threadId              = matrixTimelineThreadEventId_.trimmed();
    // When in a thread but no explicit reply, reply to the thread root.
    const auto effectiveReplyEventId = replyEventId.isEmpty() ? threadId : replyEventId;
    const auto isReplyOrThread       = !effectiveReplyEventId.isEmpty();
    const auto action = isReplyOrThread ? QStringLiteral("reply") : QStringLiteral("message");

    komai::qt_worker_task::runQueued(
      this,
      [handleId,
       roomId,
       plainBody,
       useMarkdownFormatting,
       effectiveReplyEventId,
       threadId,
       action]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok =
            effectiveReplyEventId.isEmpty()
              ? komai::MatrixBackendRuntimeService::sendRoomMessage(context,
                                                                    handleId,
                                                                    roomId,
                                                                    plainBody,
                                                                    useMarkdownFormatting,
                                                                    QStringLiteral("text"),
                                                                    &error)
              : komai::MatrixBackendRuntimeService::sendRoomReplyMessage(context,
                                                                         handleId,
                                                                         roomId,
                                                                         effectiveReplyEventId,
                                                                         plainBody,
                                                                         useMarkdownFormatting,
                                                                         QStringLiteral("text"),
                                                                         threadId,
                                                                         &error);

          return MatrixTimelineMessageSendResult{
            .handleId      = handleId,
            .roomId        = roomId,
            .targetEventId = effectiveReplyEventId,
            .action        = action,
            .error         = error,
            .ok            = ok,
          };
      },
      [](TimelineViewManager *, MatrixTimelineMessageSendResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok)
              return;

          nhlog::ui()->warn("Failed to queue matrix-sdk room {} for '{}' on handle {}: {}",
                            result.action.toStdString(),
                            result.roomId.toStdString(),
                            result.handleId,
                            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to send message: %1").arg(result.error));
      });

    bool stateChanged = clearActiveMatrixReplyState();
    stateChanged |= clearActiveMatrixThreadState();
    if (stateChanged)
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

    bool clearedReplyState = clearActiveMatrixReplyState();
    clearedReplyState |= clearActiveMatrixThreadState();
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
    const auto plainBody = emoji::replaceEmoticons(
      body.trimmed(), UserSettings::instance()->composerInputAutoReplaceEmoji());
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

    const auto roomId                = activeMatrixTimelineRoomId_;
    const auto targetEventId         = matrixTimelineEditEventId_.trimmed();
    const auto useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();
    const auto messageKind           = matrixTimelineEditMessageKind_;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, targetEventId, plainBody, useMarkdownFormatting, messageKind]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok =
            komai::MatrixBackendRuntimeService::sendRoomEditMessage(context,
                                                                    handleId,
                                                                    roomId,
                                                                    targetEventId,
                                                                    plainBody,
                                                                    useMarkdownFormatting,
                                                                    messageKind,
                                                                    &error);

          return MatrixTimelineMessageSendResult{
            .handleId      = handleId,
            .roomId        = roomId,
            .targetEventId = targetEventId,
            .action        = QStringLiteral("edit"),
            .error         = error,
            .ok            = ok,
          };
      },
      [](TimelineViewManager *, MatrixTimelineMessageSendResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok)
              return;

          nhlog::ui()->warn(
            "Failed to queue matrix-sdk room edit for '{}' on handle {} targeting '{}': {}",
            result.roomId.toStdString(),
            result.handleId,
            result.targetEventId.toStdString(),
            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to edit message: %1").arg(result.error));
      });

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

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId, trimmedReactionKey]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::toggleRoomReaction(
            context, handleId, roomId, trimmedEventId, trimmedReactionKey, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = trimmedReactionKey,
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok) {
              if (manager && manager->activeMatrixTimelineRoomId_ == result.roomId) {
                  manager->invalidateMatrixTimelineFrequentReactionsCache(result.roomId);
                  manager->refreshActiveMatrixTimelineRoomStateAsync();
              }
              return;
          }

          nhlog::ui()->warn("Failed to toggle matrix-sdk room reaction '{}' for event '{}' in "
                            "'{}' on handle {}: {}",
                            result.detail.toStdString(),
                            result.eventId.toStdString(),
                            result.roomId.toStdString(),
                            result.handleId,
                            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to react: %1").arg(result.error));
      });
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

    const auto roomId        = activeMatrixTimelineRoomId_;
    const auto trimmedReason = reason.trimmed();
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId, trimmedReason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::redactRoomEvent(
            context, handleId, roomId, trimmedEventId, trimmedReason, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              nhlog::ui()->warn(
                "Failed to redact matrix-sdk room event '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to delete message: %1").arg(result.error));
              return;
          }

          if (manager->activeMatrixTimelineRoomId_ != result.roomId ||
              !manager->matrixTimelineModel_)
              return;

          manager->matrixTimelineModel_->redactItemByEventId(result.eventId);
      });
    return true;
}

bool
TimelineViewManager::redactActiveMatrixTimelineEvents(const QStringList &eventIds,
                                                      const QString &reason)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to redact matrix-sdk room events without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto roomId        = activeMatrixTimelineRoomId_;
    const auto trimmedReason = reason.trimmed();

    QStringList trimmedIds;
    trimmedIds.reserve(eventIds.size());
    for (const auto &rawId : eventIds) {
        const auto eid = rawId.trimmed();
        if (!eid.isEmpty())
            trimmedIds.append(eid);
    }

    if (trimmedIds.isEmpty())
        return false;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedReason, trimmedIds]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          int failCount      = 0;
          QString lastError;

          for (const auto &eventId : trimmedIds) {
              QString error;
              const bool ok = komai::MatrixBackendRuntimeService::redactRoomEvent(
                context, handleId, roomId, eventId, trimmedReason, &error);

              if (!ok) {
                  ++failCount;
                  lastError = error;
                  nhlog::ui()->warn(
                    "Failed to redact matrix-sdk room event '{}' in '{}' on handle {}: {}",
                    eventId.toStdString(),
                    roomId.toStdString(),
                    handleId,
                    error.toStdString());
              }
          }

          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedIds.join(QStringLiteral(",")),
            .detail   = {},
            .error    = lastError,
            .ok       = failCount == 0,
          };
      },
      [trimmedIds](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          if (manager->activeMatrixTimelineRoomId_ == result.roomId &&
              manager->matrixTimelineModel_) {
              for (const auto &eventId : trimmedIds)
                  manager->matrixTimelineModel_->redactItemByEventId(eventId);
          }

          if (result.ok)
              return;

          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to delete some messages: %1").arg(result.error));
      });
    return true;
}

bool
TimelineViewManager::redactActiveMatrixTimelineEventsByUser(const QString &userId,
                                                            const QString &reason)
{
    if (!matrixTimelineModel_ || userId.isEmpty())
        return false;

    QStringList eventIds;
    const auto items = matrixTimelineModel_->visibleItemsSnapshot();
    for (const auto &item : items) {
        if (item.senderId == userId && !item.eventId.isEmpty())
            eventIds.append(item.eventId);
    }

    if (eventIds.isEmpty())
        return false;

    return redactActiveMatrixTimelineEvents(eventIds, reason);
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

    queueMatrixRoomReadMarker(handleId, activeMatrixTimelineRoomId_, trimmedEventId);

    return true;
}

void
TimelineViewManager::queueMatrixRoomReadMarker(uint64_t handleId,
                                               const QString &roomId,
                                               const QString &eventId)
{
    if (handleId == 0 || roomId.isEmpty() || eventId.isEmpty())
        return;

    const auto inflightIt = matrixReadMarkerInFlightEventIdsByRoom_.constFind(roomId);
    if (inflightIt != matrixReadMarkerInFlightEventIdsByRoom_.cend() &&
        inflightIt.value() == eventId) {
        return;
    }

    const auto pendingIt = matrixReadMarkerPendingEventIdsByRoom_.constFind(roomId);
    if (pendingIt != matrixReadMarkerPendingEventIdsByRoom_.cend() && pendingIt.value() == eventId)
        return;

    matrixReadMarkerPendingHandlesByRoom_.insert(roomId, handleId);
    matrixReadMarkerPendingEventIdsByRoom_.insert(roomId, eventId);
    dispatchPendingMatrixReadMarker(roomId);
}

void
TimelineViewManager::dispatchPendingMatrixReadMarker(const QString &roomId)
{
    if (roomId.isEmpty() || matrixReadMarkerInFlightEventIdsByRoom_.contains(roomId) ||
        !matrixReadMarkerPendingEventIdsByRoom_.contains(roomId) ||
        !matrixReadMarkerPendingHandlesByRoom_.contains(roomId)) {
        return;
    }

    const auto handleId = matrixReadMarkerPendingHandlesByRoom_.take(roomId);
    const auto eventId  = matrixReadMarkerPendingEventIdsByRoom_.take(roomId);
    if (handleId == 0 || eventId.isEmpty())
        return;

    matrixReadMarkerInFlightEventIdsByRoom_.insert(roomId, eventId);

    QPointer<TimelineViewManager> guard(this);
    std::thread([guard, handleId, roomId, eventId]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const bool ok = komai::MatrixBackendRuntimeService::markRoomEventAsRead(
          context, handleId, roomId, eventId, &error);

        if (!guard)
            return;

        QMetaObject::invokeMethod(
          guard,
          [guard, handleId, roomId, eventId, ok, error]() {
              if (!guard)
                  return;

              const auto inflightIt =
                guard->matrixReadMarkerInFlightEventIdsByRoom_.constFind(roomId);
              const bool isCurrentInflight =
                inflightIt != guard->matrixReadMarkerInFlightEventIdsByRoom_.cend() &&
                inflightIt.value() == eventId;
              if (isCurrentInflight)
                  guard->matrixReadMarkerInFlightEventIdsByRoom_.remove(roomId);

              if (!ok) {
                  nhlog::ui()->warn(
                    "Failed to mark matrix-sdk room event '{}' as read in '{}' on handle {}: {}",
                    eventId.toStdString(),
                    roomId.toStdString(),
                    handleId,
                    error.toStdString());

                  if (guard->activeMatrixTimelineRoomId_ == roomId) {
                      if (auto *mainWindow = MainWindow::instance()) {
                          mainWindow->showNotification(
                            TimelineViewManager::tr("Failed to mark message as read: %1")
                              .arg(error));
                      }
                  }
              }

              guard->dispatchPendingMatrixReadMarker(roomId);
          },
          Qt::QueuedConnection);
    }).detach();
}

void
TimelineViewManager::clearMatrixReadMarkerQueue()
{
    matrixReadMarkerPendingHandlesByRoom_.clear();
    matrixReadMarkerPendingEventIdsByRoom_.clear();
    matrixReadMarkerInFlightEventIdsByRoom_.clear();
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

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId, reason, score]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::reportRoomEvent(
            context, handleId, roomId, trimmedEventId, reason, score, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok)
              return;

          nhlog::ui()->warn("Failed to report matrix-sdk room event '{}' in '{}' on handle {}: {}",
                            result.eventId.toStdString(),
                            result.roomId.toStdString(),
                            result.handleId,
                            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to report message: %1").arg(result.error));
      });
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
        const auto sourceRoomId          = activeMatrixTimelineRoomId_;
        const auto useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();
        const auto normalizedKind        = normalizedMatrixMessageKind(itemKind);
        komai::qt_worker_task::runQueued(
          this,
          [handleId,
           sourceRoomId,
           trimmedTargetRoomId,
           trimmedEventId,
           body = item->body,
           useMarkdownFormatting,
           normalizedKind]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              const bool ok =
                komai::MatrixBackendRuntimeService::sendRoomMessage(context,
                                                                    handleId,
                                                                    trimmedTargetRoomId,
                                                                    body,
                                                                    useMarkdownFormatting,
                                                                    normalizedKind,
                                                                    &error);
              return MatrixTimelineEventActionResult{
                .handleId = handleId,
                .roomId   = sourceRoomId,
                .eventId  = trimmedEventId,
                .detail   = trimmedTargetRoomId,
                .error    = error,
                .ok       = ok,
              };
          },
          [](TimelineViewManager *, MatrixTimelineEventActionResult result) {
              auto *mainWindow = MainWindow::instance();
              if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
                  return;

              if (result.ok)
                  return;

              nhlog::ui()->warn("Failed to forward matrix-sdk room event '{}' from '{}' to '{}' "
                                "on handle {}: {}",
                                result.eventId.toStdString(),
                                result.roomId.toStdString(),
                                result.detail.toStdString(),
                                result.handleId,
                                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to forward message: %1").arg(result.error));
          });
        return true;
    }

    // For all other event types (media, stickers, etc.), forward by extracting the
    // raw event content JSON and resending it as-is.  This preserves all metadata
    // (dimensions, duration, thumbnail, blurhash, etc.) and avoids re-uploading
    // since the mxc:// URLs are already server-hosted.
    const auto sourceRoomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, sourceRoomId, trimmedTargetRoomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;

          auto content =
            komai::MatrixBackendRuntimeService::fetchActiveRoomEventContentForForwarding(
              context, handleId, sourceRoomId, trimmedEventId, &error);
          if (!content) {
              return MatrixTimelineEventActionResult{
                .handleId = handleId,
                .roomId   = sourceRoomId,
                .eventId  = trimmedEventId,
                .detail   = trimmedTargetRoomId,
                .error    = error,
                .ok       = false,
              };
          }

          const bool ok =
            komai::MatrixBackendRuntimeService::sendRoomMessageLikeEventJson(context,
                                                                             handleId,
                                                                             trimmedTargetRoomId,
                                                                             content->eventType,
                                                                             content->contentJson,
                                                                             &error);

          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = sourceRoomId,
            .eventId  = trimmedEventId,
            .detail   = trimmedTargetRoomId,
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok)
              return;

          nhlog::ui()->warn("Failed to forward matrix-sdk room event '{}' from '{}' to '{}' "
                            "on handle {}: {}",
                            result.eventId.toStdString(),
                            result.roomId.toStdString(),
                            result.detail.toStdString(),
                            result.handleId,
                            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to forward message: %1").arg(result.error));
      });
    return true;
}

bool
TimelineViewManager::forwardActiveMatrixTimelineEvents(const QStringList &eventIds,
                                                       const QString &targetRoomId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        nhlog::ui()->warn("Refusing to forward matrix-sdk room events without an active runtime "
                          "handle or selected matrix room");
        return false;
    }

    const auto trimmedTargetRoomId = targetRoomId.trimmed();
    if (trimmedTargetRoomId.isEmpty())
        return false;

    if (!matrixTimelineModel_) {
        nhlog::ui()->warn("Refusing to forward matrix-sdk room events without an active "
                          "timeline model");
        return false;
    }

    struct ForwardEntry
    {
        QString eventId;
        QString itemKind;
        QString body;
    };

    const auto sourceRoomId          = activeMatrixTimelineRoomId_;
    const auto useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();
    QVector<ForwardEntry> entries;
    entries.reserve(eventIds.size());

    for (const auto &rawId : eventIds) {
        const auto eid = rawId.trimmed();
        if (eid.isEmpty())
            continue;

        const auto item = matrixTimelineModel_->itemByEventId(eid);
        if (!item) {
            nhlog::ui()->warn("Skipping unknown matrix-sdk room event '{}' during batch forward",
                              eid.toStdString());
            continue;
        }

        entries.append(ForwardEntry{
          .eventId  = eid,
          .itemKind = item->itemKind.trimmed().toLower(),
          .body     = item->body,
        });
    }

    if (entries.isEmpty())
        return false;

    komai::qt_worker_task::runQueued(
      this,
      [handleId,
       sourceRoomId,
       trimmedTargetRoomId,
       useMarkdownFormatting,
       entries = std::move(entries)]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          int failCount      = 0;
          QString lastError;

          for (const auto &entry : entries) {
              QString error;
              bool ok = false;

              if (isForwardableActiveMatrixTimelineTextKind(entry.itemKind)) {
                  const auto normalizedKind =
                    entry.itemKind == QStringLiteral("notice")
                      ? QStringLiteral("notice")
                      : (entry.itemKind == QStringLiteral("emote") ? QStringLiteral("emote")
                                                                   : QStringLiteral("text"));
                  ok = komai::MatrixBackendRuntimeService::sendRoomMessage(context,
                                                                           handleId,
                                                                           trimmedTargetRoomId,
                                                                           entry.body,
                                                                           useMarkdownFormatting,
                                                                           normalizedKind,
                                                                           &error);
              } else {
                  auto content =
                    komai::MatrixBackendRuntimeService::fetchActiveRoomEventContentForForwarding(
                      context, handleId, sourceRoomId, entry.eventId, &error);
                  if (content) {
                      ok = komai::MatrixBackendRuntimeService::sendRoomMessageLikeEventJson(
                        context,
                        handleId,
                        trimmedTargetRoomId,
                        content->eventType,
                        content->contentJson,
                        &error);
                  }
              }

              if (!ok) {
                  ++failCount;
                  lastError = error;
                  nhlog::ui()->warn(
                    "Failed to forward matrix-sdk room event '{}' from '{}' to '{}': {}",
                    entry.eventId.toStdString(),
                    sourceRoomId.toStdString(),
                    trimmedTargetRoomId.toStdString(),
                    error.toStdString());
              }
          }

          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = sourceRoomId,
            .eventId  = QString::number(entries.size()),
            .detail   = trimmedTargetRoomId,
            .error    = lastError,
            .ok       = failCount == 0,
          };
      },
      [](TimelineViewManager *, MatrixTimelineEventActionResult result) {
          if (result.ok)
              return;

          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to forward some messages: %1").arg(result.error));
      });
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

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::pinRoomEvent(
            context, handleId, roomId, trimmedEventId, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              nhlog::ui()->warn("Failed to pin matrix-sdk room event '{}' in '{}' on handle {}: {}",
                                result.eventId.toStdString(),
                                result.roomId.toStdString(),
                                result.handleId,
                                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to pin message: %1").arg(result.error));
              return;
          }

          if (manager->activeMatrixTimelineRoomId_ != result.roomId)
              return;

          if (!manager->matrixTimelinePinnedEventIds_.contains(result.eventId)) {
              manager->matrixTimelinePinnedEventIds_.push_back(result.eventId);
              emit manager->matrixTimelineStateChanged();
          }
      });
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

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::unpinRoomEvent(
            context, handleId, roomId, trimmedEventId, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              nhlog::ui()->warn(
                "Failed to unpin matrix-sdk room event '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to unpin message: %1").arg(result.error));
              return;
          }

          if (manager->activeMatrixTimelineRoomId_ != result.roomId)
              return;

          if (manager->matrixTimelinePinnedEventIds_.removeAll(result.eventId) > 0)
              emit manager->matrixTimelineStateChanged();
      });
    return true;
}

bool
TimelineViewManager::requestRawMessageDialogForActiveMatrixTimelineEvent(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto roomId    = activeMatrixTimelineRoomId_;
    const auto themeSlug = UserSettings::instance()->uiThemeSlug();

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const auto dialogData =
            komai::MatrixBackendRuntimeService::fetchActiveRoomRawEventDialogData(
              context, handleId, roomId, trimmedEventId, &error);
          return MatrixTimelineRawMessageFetchResult{
            .handleId      = handleId,
            .roomId        = roomId,
            .eventId       = trimmedEventId,
            .prettyJson    = dialogData ? dialogData->prettyJson : QString(),
            .body          = dialogData ? dialogData->body : QString(),
            .formattedBody = dialogData ? dialogData->formattedBody : QString(),
            .error         = error,
            .ok            = dialogData.has_value(),
          };
      },
      [themeSlug](TimelineViewManager *manager, MatrixTimelineRawMessageFetchResult result) {
          QVariantMap dialogData;

          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              nhlog::ui()->warn(
                "Failed to fetch raw JSON for matrix-sdk room event '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              emit manager->activeMatrixTimelineRawMessageDialogReady(result.eventId, dialogData);
              return;
          }

          const auto timelinePalette = Theme::paletteFromTheme(themeSlug);
          const auto renderedRawMessage =
            timeline::formattedcode::formatRawJsonForDialog(result.prettyJson, timelinePalette);
          dialogData.insert(QStringLiteral("renderedRawMessage"), renderedRawMessage);
          dialogData.insert(QStringLiteral("rawMessageJson"), result.prettyJson);
          dialogData.insert(QStringLiteral("rawMessageBody"), result.body);
          dialogData.insert(QStringLiteral("rawMessageFormattedBody"), result.formattedBody);

          emit manager->activeMatrixTimelineRawMessageDialogReady(result.eventId, dialogData);
      });

    return true;
}

bool
TimelineViewManager::requestReadReceiptsModelForActiveMatrixTimelineEvent(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const auto receipts = komai::MatrixBackendRuntimeService::fetchRoomReadReceipts(
            context, handleId, roomId, trimmedEventId, &error);
          return MatrixTimelineReadReceiptsFetchResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .receipts = receipts.value_or(QVector<komai::MatrixReadReceiptEntry>{}),
            .error    = error,
            .ok       = receipts.has_value(),
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineReadReceiptsFetchResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          QObject *model = nullptr;
          if (!result.ok) {
              nhlog::ui()->warn("Failed to fetch matrix-sdk room read receipts for event '{}' in "
                                "'{}' on handle {}: {}",
                                result.eventId.toStdString(),
                                result.roomId.toStdString(),
                                result.handleId,
                                result.error.toStdString());
              emit manager->activeMatrixTimelineReadReceiptsReady(result.eventId, model);
              return;
          }

          QVector<ReadReceiptEntry> entries;
          entries.reserve(result.receipts.size());
          for (const auto &entry : result.receipts) {
              entries.push_back(ReadReceiptEntry{
                .mxid         = entry.userId,
                .displayName  = entry.displayName,
                .avatarUrl    = entry.avatarUrl,
                .rawTimestamp = entry.timestamp == 0 ? QDateTime{}
                                                     : QDateTime::fromMSecsSinceEpoch(
                                                         static_cast<qint64>(entry.timestamp)),
              });
          }

          model = new ReadReceiptsProxy{std::move(entries), result.roomId};
          QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
          emit manager->activeMatrixTimelineReadReceiptsReady(result.eventId, model);
      });

    return true;
}

bool
TimelineViewManager::openActiveMatrixAttachmentSelection()
{
    const auto targetRoomId = activeMatrixTimelineRoomId_.trimmed();
    if (targetRoomId.isEmpty()) {
        nhlog::ui()->warn("Refusing to queue matrix-sdk room attachment without a selected room");
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

    nhlog::ui()->info("Clipboard paste: formats=[{}], hasImage={}",
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
            nhlog::ui()->warn("Failed to create temp file for clipboard paste: {}",
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
        nhlog::ui()->warn("Refusing to queue matrix-sdk room attachment without an active "
                          "runtime handle or selected matrix room (room='{}')",
                          targetRoomId.toStdString());
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

    QMimeDatabase mimeDatabase;
    QStringList normalizedFilePaths;
    normalizedFilePaths.reserve(filePaths.size());

    for (const auto &filePath : filePaths) {
        const QFileInfo info(filePath);
        if (!info.exists() || !info.isFile())
            continue;

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
          .mimeType     = effectiveMimeType,
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
    clearedState |= clearActiveMatrixThreadState();
    startNextPendingMatrixAttachment();
    if (clearedState)
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

    if (waitingForFirstSync_) {
        // First snapshot: process immediately so the room list appears without delay.
        nhlog::ui()->info("Clearing waitingForFirstSync from first matrix-sdk room-list snapshot "
                          "for handle {}",
                          handleId);
        waitingForFirstSync_ = false;
        emit waitingForFirstSyncChanged(false);
        rooms_->refreshMatrixBackendRooms();
        scheduleMatrixSidebarRefresh();
        return;
    }

    // Subsequent snapshots: coalesce rapid updates so the UI thread is not
    // blocked by repeated full model resets during the initial sync burst.
    matrixRoomListRefreshPending_ = true;
    if (!matrixRoomListRefreshQueued_) {
        matrixRoomListRefreshQueued_ = true;
        QTimer::singleShot(200, this, [this]() {
            matrixRoomListRefreshQueued_ = false;
            if (!matrixRoomListRefreshPending_)
                return;
            matrixRoomListRefreshPending_ = false;
            rooms_->refreshMatrixBackendRooms();
            scheduleMatrixSidebarRefresh();
        });
    }
}

void
TimelineViewManager::handleMatrixBackendNotificationReceived(std::uint64_t handleId,
                                                             const QString &roomId,
                                                             const QString &eventId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId || !rooms_)
        return;

    rooms_->queueMatrixNotificationFetch(handleId, roomId, eventId);
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

    if (matrixTimelinePendingJumpRoomId_ == roomId)
        matrixTimelinePendingJumpAwaitingSnapshot_ = false;

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

void
TimelineViewManager::handleMatrixBackendSyncStopped(std::uint64_t handleId,
                                                    const QString &reason,
                                                    bool isAuthError)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    nhlog::ui()->warn("Matrix-sdk sync stopped for handle {} (auth_error={}): {}",
                      handleId,
                      isAuthError,
                      reason.toStdString());

    if (isAuthError) {
        auto *chatPage = qobject_cast<ChatPage *>(parent());
        if (chatPage)
            emit chatPage->dropToLoginPageCb(
              tr("Your session has expired. Please sign in again.\n\n(%1)").arg(reason));
    }
}

void
TimelineViewManager::handleMatrixBackendSyncConnectionStateChanged(std::uint64_t handleId,
                                                                   bool isConnected)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    if (isConnected_ == isConnected)
        return;

    isConnected_ = isConnected;
    emit isConnectedChanged(isConnected_);
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
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const bool ok =
          komai::MatrixBackendRuntimeService::sendRoomAttachment(context,
                                                                 attachment.handleId,
                                                                 attachment.roomId,
                                                                 attachment.filePath,
                                                                 attachment.filename,
                                                                 attachment.body,
                                                                 attachment.replyEventId,
                                                                 attachment.threadId,
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
TimelineViewManager::queueActiveMatrixThread(const QString &threadEventId)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedThreadEventId = threadEventId.trimmed();
    if (trimmedThreadEventId.isEmpty())
        return false;

    if (setActiveMatrixThreadState(trimmedThreadEventId))
        emit matrixTimelineStateChanged();
    focusMessageInput();
    return true;
}

void
TimelineViewManager::clearActiveMatrixThread()
{
    if (!clearActiveMatrixThreadState())
        return;

    emit matrixTimelineStateChanged();
}

bool
TimelineViewManager::setActiveMatrixThreadState(const QString &threadEventId)
{
    const auto trimmed = threadEventId.trimmed();
    if (matrixTimelineThreadEventId_ == trimmed)
        return false;

    matrixTimelineThreadEventId_ = trimmed;
    return true;
}

bool
TimelineViewManager::clearActiveMatrixThreadState()
{
    if (matrixTimelineThreadEventId_.isEmpty())
        return false;

    matrixTimelineThreadEventId_.clear();
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

void
TimelineViewManager::handleMatrixBackendTypingUsersUpdated(std::uint64_t handleId,
                                                           const QString &roomId,
                                                           const QStringList &displayNames)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;
    if (activeMatrixTimelineRoomId_ != roomId)
        return;

    if (matrixTimelineTypingUsers_ != displayNames) {
        matrixTimelineTypingUsers_ = displayNames;
        emit matrixTimelineTypingUsersChanged();
    }
}

void
TimelineViewManager::sendActiveMatrixTypingNotice(bool typing)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return;

    auto *mainWindow = MainWindow::instance();
    if (!mainWindow)
        return;
    const auto handleId = mainWindow->matrixBackendHandleId();
    if (handleId == 0)
        return;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, typing]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          komai::MatrixBackendRuntimeService::sendTypingNotice(context, handleId, roomId, typing);
      },
      [](TimelineViewManager *) {});
}
