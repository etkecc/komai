// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#ifdef __GLIBC__
#include <malloc.h>
#endif

#include <QByteArray>
#include <QClipboard>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QUrl>

#include "chat/ChatPage.h"
#include "encryption/VerificationManager.h"
#include "imagepacks/ImagePackListModel.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "models/InviteesModel.h"
#include "models/MemberList.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/CommunitiesModel.h"
#include "timeline/PresenceEmitter.h"
#include "timeline/RoomlistModel.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "ui/MainWindow.h"
#include "ui/RoomSettings.h"
#include "ui/UserProfile.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

namespace {
bool
isTruthyEnvValue(const QByteArray &value)
{
    const auto normalized = value.trimmed().toLower();
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

} // namespace

TimelineViewManager::TimelineViewManager(CallManager *, ChatPage *parent)
  : QObject(parent)
  , rooms_(new RoomlistModel(this))
  , frooms_(new FilteredRoomlistModel(this->rooms_))
  , communities_(new CommunitiesModel(this))
  , verificationManager_(new VerificationManager(this))
  , presenceEmitter(new PresenceEmitter(this))
  , matrixTimelineModel_(new komai::MatrixTimelineModel(this))
{
    instance_              = this;
    roomSwitchPerfEnabled_ = isTruthyEnvValue(qgetenv("KOMAI_ROOM_SWITCH_PERF")) ||
                             isTruthyEnvValue(qgetenv("KOMAI_PERF_ROOM_SWITCH"));

    if (roomSwitchPerfEnabled_) {
        komai::logging::ui()->info("Room-switch performance tracing enabled (set by "
                                   "KOMAI_ROOM_SWITCH_PERF/KOMAI_PERF_ROOM_SWITCH).");
        komai::logging::ui()->flush();
    }

    connect(this->communities_,
            &CommunitiesModel::currentFilterIdChanged,
            frooms_,
            &FilteredRoomlistModel::updateFilterTag);
    connect(this->communities_,
            &CommunitiesModel::globalExcludesChanged,
            frooms_,
            &FilteredRoomlistModel::updateGlobalExcludes);

    updateColorPalette();

    connect(UserSettings::instance().get(),
            &UserSettings::uiThemeSlugChanged,
            this,
            &TimelineViewManager::updateColorPalette);
    connect(UserSettings::instance().get(),
            &UserSettings::timelineUserColorCodingPolicyChanged,
            this,
            &TimelineViewManager::updateColorPalette);
    connect(parent, &ChatPage::loggedOut, this, [this]() {
        clearCurrentMatrixTimeline();
        setIgnoredUsers({});
        if (presenceEmitter)
            presenceEmitter->clear();
        waitingForFirstSync_ = true;
        emit waitingForFirstSyncChanged(true);
    });
    connect(parent, &ChatPage::connectionLost, this, [this] {
        isConnected_ = false;
        updateConnectedState();
    });
    connect(parent, &ChatPage::connectionRestored, this, [this] {
        isConnected_ = true;
        updateConnectedState();
    });
    connect(rooms_, &RoomlistModel::spaceSelected, communities_, [this](QString roomId) {
        communities_->setCurrentFilterId("space:" + roomId);
    });
    connect(rooms_, &RoomlistModel::currentRoomIdChanged, this, [this](const QString &) {
        if (!matrixTimelineAutoSelectionSuppressed_)
            scheduleCurrentMatrixTimelineSelectionUpdate();
    });
    connect(matrixTimelineModel_,
            &komai::MatrixTimelineModel::countChanged,
            this,
            &TimelineViewManager::matrixTimelineStateChanged);

    // Seed navigation history with the initial state (no room open).
    navHistory_.push(communities_->currentFilterId(), QString());

    // Navigation history: record filter and room changes.
    // Filter changes capture the current room too (the full state), but are marked as
    // filter-only so the skip logic in back()/forward() can skip intermediate entries
    // where only the filter changed (same room was still displayed).
    connect(
      communities_, &CommunitiesModel::currentFilterIdChanged, this, [this](QString filterId) {
          if (navigating_)
              return;
          navHistory_.push(filterId, rooms_->currentRoomId(), true);
      });
    connect(rooms_, &RoomlistModel::currentRoomIdChanged, this, [this](QString roomId) {
        if (navigating_)
            return;
        navHistory_.push(communities_->currentFilterId(), roomId);
    });
}

TimelineViewManager *
TimelineViewManager::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    // The instance has to exist before it is used. We cannot replace it.
    Q_ASSERT(instance_);

    // The engine has to have the same thread affinity as the singleton.
    Q_ASSERT(qmlEngine->thread() == instance_->thread());

    // There can only be one engine accessing the singleton.
    static QJSEngine *s_engine = nullptr;
    if (s_engine)
        Q_ASSERT(qmlEngine == s_engine);
    else
        s_engine = qmlEngine;

    QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
    return instance_;
}

void
TimelineViewManager::clearAll()
{
    clearCurrentMatrixTimeline();
    rooms_->clear();
}

void
TimelineViewManager::handleMatrixBackendInitialSyncReady(std::uint64_t handleId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    if (!waitingForFirstSync_)
        return;

    waitingForFirstSync_ = false;
    emit waitingForFirstSyncChanged(false);
}

void
TimelineViewManager::refreshRoomVersionsCapability()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || roomVersionsCapabilityInFlight_)
        return;

    roomVersionsCapabilityInFlight_ = true;

    komai::qt_worker_task::runQueued(
      this,
      [handleId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          auto result = komai::MatrixBackendRuntimeService::fetchRoomVersionsCapability(
            context, handleId, &error);
          return std::make_pair(std::move(result), error);
      },
      [](TimelineViewManager *self,
         const std::pair<std::optional<komai::MatrixRoomVersionsCapability>, QString> &outcome) {
          self->roomVersionsCapabilityInFlight_ = false;
          const auto &[result, error]           = outcome;
          if (!result.has_value()) {
              if (!error.isEmpty()) {
                  komai::logging::ui()->warn("Failed to fetch room versions capability: {}",
                                             error.toStdString());
              }
              return;
          }

          const auto &cap = *result;
          QStringList stable;
          stable.reserve(cap.stableVersions.size());
          for (const auto &v : cap.stableVersions)
              stable.push_back(v);

          if (self->defaultRoomVersion_ == cap.defaultVersion &&
              self->stableRoomVersions_ == stable && self->roomVersionsCapabilityLoaded_) {
              return;
          }

          self->defaultRoomVersion_           = cap.defaultVersion;
          self->stableRoomVersions_           = std::move(stable);
          self->roomVersionsCapabilityLoaded_ = true;
          emit self->roomVersionsCapabilityChanged();
      });
}

void
TimelineViewManager::handleMatrixBackendIgnoredUsersUpdated(std::uint64_t handleId,
                                                            const QVector<QString> &ignoredUsers)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    setIgnoredUsers(ignoredUsers);
}

bool
TimelineViewManager::setIgnoredUsers(QVector<QString> ignoredUsers)
{
    std::sort(ignoredUsers.begin(), ignoredUsers.end());
    ignoredUsers.erase(std::unique(ignoredUsers.begin(), ignoredUsers.end()), ignoredUsers.end());

    if (ignoredUsers_ == ignoredUsers)
        return false;

    ignoredUsers_ = std::move(ignoredUsers);
    emit ignoredUsersChanged(ignoredUsers_);
    return true;
}

QString
TimelineViewManager::escapeEmoji(QString str) const
{
    return utils::replaceEmoji(str);
}

int
TimelineViewManager::emojiOnlyCodepointCount(const QString &body) const
{
    return utils::emojiOnlyCodepointCount(body);
}

void
TimelineViewManager::markRoomSwitchRequested(const QString &roomId, const QString &reason)
{
    if (!roomSwitchPerfEnabled_ || roomId.isEmpty())
        return;

    roomSwitchPerfSwitchId_++;
    roomSwitchPerfActiveRoomId_ = roomId;
    roomSwitchPerfTimer_.restart();

    komai::logging::ui()->info(
      "[perf][room-switch] switch_id={} room_id={} phase=request reason={}",
      roomSwitchPerfSwitchId_,
      roomId.toStdString(),
      reason.toStdString());
    komai::logging::ui()->flush();
}

void
TimelineViewManager::markRoomSwitchPhaseCpp(const QString &roomId, const QString &phase)
{
    logRoomSwitchPhase(roomId, phase, "cpp");
}

void
TimelineViewManager::markRoomSwitchPhase(const QString &roomId, const QString &phase)
{
    logRoomSwitchPhase(roomId, phase, "qml");
}

QAbstractItemModel *
TimelineViewManager::matrixTimelineModel() const
{
    return matrixTimelineModel_;
}

QAbstractItemModel *
TimelineViewManager::ensureModelForRoom(const QString &roomId)
{
    const auto trimmed = roomId.trimmed();
    if (trimmed.isEmpty())
        return nullptr;

    auto it = perRoomModels_.find(trimmed);
    if (it != perRoomModels_.end())
        return *it;

    auto *model = new komai::MatrixTimelineModel(this);
    model->setRoomId(trimmed);
    perRoomModels_.insert(trimmed, model);
    komai::logging::ui()->info("Created per-room timeline model for '{}' (pool size: {})",
                               trimmed.toStdString(),
                               perRoomModels_.size());
    return model;
}

void
TimelineViewManager::releaseModelForRoom(const QString &roomId)
{
    const auto trimmed = roomId.trimmed();
    if (trimmed.isEmpty())
        return;

    // Never release the model for the currently active room.
    if (trimmed == activeMatrixTimelineRoomId_)
        return;

    auto it = perRoomModels_.find(trimmed);
    if (it == perRoomModels_.end())
        return;

    auto *model = *it;
    perRoomModels_.erase(it);
    model->deleteLater();

    // Stop the concurrent Rust timeline loop for this room.
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId) {
        QString error;
        if (!komai::MatrixBackendRuntimeService::stopRoomTimeline(handleId, trimmed, &error))
            komai::logging::ui()->warn("Failed to stop room timeline for '{}': {}",
                                       trimmed.toStdString(),
                                       error.toStdString());
    }

    komai::logging::ui()->info("Released per-room timeline model for '{}' (pool size: {})",
                               trimmed.toStdString(),
                               perRoomModels_.size());
}

void
TimelineViewManager::trimProcessMemory()
{
#ifdef __GLIBC__
    ::malloc_trim(0);
    komai::logging::ui()->info("Trimmed process memory (malloc_trim)");
#endif
}

int
TimelineViewManager::matrixTimelineItemCount() const
{
    return matrixTimelineModel_ ? matrixTimelineModel_->count() : 0;
}

void
TimelineViewManager::copyMatrixEventLink(const QString &roomId, const QString &eventId) const
{
    const auto trimmedRoomId  = roomId.trimmed();
    const auto trimmedEventId = eventId.trimmed();
    if (trimmedRoomId.isEmpty() || trimmedEventId.isEmpty())
        return;

    const auto link = QStringLiteral("https://matrix.to/#/%1/%2")
                        .arg(QString(QUrl::toPercentEncoding(trimmedRoomId)),
                             QString(QUrl::toPercentEncoding(trimmedEventId)));
    QGuiApplication::clipboard()->setText(link);
}

bool
TimelineViewManager::perfUiFlagEnabled(const QString &flag) const
{
    if (flag == QLatin1String("disable_composer"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_COMPOSER"));

    if (flag == QLatin1String("disable_room_header"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_ROOM_HEADER"));

    if (flag == QLatin1String("disable_timeline_effects"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_EFFECTS"));

    if (flag == QLatin1String("disable_timeline_list"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_LIST"));

    if (flag == QLatin1String("disable_timeline_section_headers"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_SECTION_HEADERS"));

    if (flag == QLatin1String("disable_timeline_avatars"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_AVATARS"));

    // Diagnostic: turns the timeline ListView's delegate reuse off, to tell
    // recycling bugs apart from rendering ones. `flushDelegateReusePool()` only
    // toggles reuse when it is already on, so this stays off once set.
    if (flag == QLatin1String("disable_delegate_reuse"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_DELEGATE_REUSE"));

    if (flag == QLatin1String("disable_timeline_reactions"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_REACTIONS"));

    if (flag == QLatin1String("disable_timeline_hover"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_HOVER"));

    if (flag == QLatin1String("disable_timeline_interaction"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_INTERACTION"));

    if (flag == QLatin1String("disable_timeline_metadata"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_METADATA"));

    if (flag == QLatin1String("disable_timeline_rich_text"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_RICH_TEXT"));

    if (flag == QLatin1String("disable_timeline_bubbles"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_BUBBLES"));

    if (flag == QLatin1String("minimal_text_bubbles"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_MINIMAL_TEXT_BUBBLES"));

    return false;
}

void
TimelineViewManager::logRoomSwitchPhase(const QString &roomId,
                                        const QString &phase,
                                        const QString &source)
{
    if (!roomSwitchPerfEnabled_ || roomId.isEmpty() || phase.isEmpty())
        return;

    const qint64 elapsedMs = roomSwitchPerfTimer_.isValid() ? roomSwitchPerfTimer_.elapsed() : -1;
    const bool activeMatch = roomId == roomSwitchPerfActiveRoomId_;

    if (activeMatch) {
        komai::logging::ui()->info(
          "[perf][room-switch] switch_id={} room_id={} phase={} source={} elapsed_ms={}",
          roomSwitchPerfSwitchId_,
          roomId.toStdString(),
          phase.toStdString(),
          source.toStdString(),
          elapsedMs);
        komai::logging::ui()->flush();
    } else {
        komai::logging::ui()->info("[perf][room-switch] switch_id={} room_id={} phase={} source={} "
                                   "elapsed_ms={} active_match=false active_room_id={}",
                                   roomSwitchPerfSwitchId_,
                                   roomId.toStdString(),
                                   phase.toStdString(),
                                   source.toStdString(),
                                   elapsedMs,
                                   roomSwitchPerfActiveRoomId_.toStdString());
        komai::logging::ui()->flush();
    }
}

void
TimelineViewManager::navigateBack()
{
    komai::logging::ui()->info("[nav-history] navigateBack called");
    auto entry = navHistory_.back(communities_->currentFilterId(), rooms_->currentRoomId());
    if (!entry) {
        komai::logging::ui()->info("[nav-history] navigateBack: no entry to restore");
        return;
    }

    komai::logging::ui()->info("[nav-history] navigateBack restoring filter='{}' room='{}'",
                               entry->filterId.toStdString(),
                               entry->roomId.toStdString());
    navigating_ = true;
    communities_->setCurrentFilterId(entry->filterId);
    rooms_->setCurrentRoom(entry->roomId);
    navigating_ = false;
}

void
TimelineViewManager::navigateForward()
{
    komai::logging::ui()->info("[nav-history] navigateForward called");
    auto entry = navHistory_.forward(communities_->currentFilterId(), rooms_->currentRoomId());
    if (!entry) {
        komai::logging::ui()->info("[nav-history] navigateForward: no entry to restore");
        return;
    }

    komai::logging::ui()->info("[nav-history] navigateForward restoring filter='{}' room='{}'",
                               entry->filterId.toStdString(),
                               entry->roomId.toStdString());
    navigating_ = true;
    communities_->setCurrentFilterId(entry->filterId);
    rooms_->setCurrentRoom(entry->roomId);
    navigating_ = false;
}

void
TimelineViewManager::updateConnectedState()
{
    if (isConnected_ == lastConnectedEmitted_)
        return;
    lastConnectedEmitted_ = isConnected_;

    komai::logging::net()->info("Connectivity changed: connected={}", isConnected_);

    if (isConnected_) {
        // Connectivity is back: clear the media-fetch backoff so failed
        // avatar/thumbnail Images that retry on this signal get a real fetch.
        MxcImageProvider::resetFetchBackoff();
    }

    emit isConnectedChanged(isConnected_);
}

#include "moc_TimelineViewManager.cpp"
