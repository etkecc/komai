// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <QQuickItem>
#include <QTimer>

#include "RoomlistModel.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "models/InviteesModel.h"
#include "models/MemberList.h"
#include "timeline/CommunitiesModel.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "ui/MainWindow.h"
#include "ui/RoomSettings.h"
#include "ui/UserProfile.h"
#include "utils/QtWorkerTask.h"
#include "voip/WebRTCSession.h"

namespace {
constexpr auto kMatrixPendingJumpPageSize        = 50;
constexpr auto kMatrixPendingJumpMaxPageRequests = 20;

}

void
TimelineViewManager::scheduleMatrixSidebarRefresh()
{
    if (!communities_ || matrixSidebarRefreshQueued_)
        return;

    matrixSidebarRefreshQueued_ = true;
    QTimer::singleShot(0, this, [this] {
        matrixSidebarRefreshQueued_ = false;
        if (!communities_)
            return;

        communities_->initializeSidebar();
    });
}

void
TimelineViewManager::openRoomSettings(QString room_id)
{
    openRoomInfo(room_id, QStringLiteral("settings"));
}

void
TimelineViewManager::openRoomInfo(const QString &roomId, const QString &initialTab)
{
    auto *settings = new RoomSettings(roomId);
    auto *members  = new MemberList(roomId);
    QQmlEngine::setObjectOwnership(settings, QQmlEngine::JavaScriptOwnership);
    QQmlEngine::setObjectOwnership(members, QQmlEngine::JavaScriptOwnership);
    emit openRoomInfoDialog(settings, members, nullptr, initialTab);
}

void
TimelineViewManager::performRoomUpgrade(const QString &roomId,
                                        const QString &newVersion,
                                        const QStringList &additionalCreators)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || roomId.isEmpty() || newVersion.isEmpty()) {
        if (mainWindow)
            mainWindow->showNotification(tr("Cannot upgrade room: backend not ready."));
        return;
    }

    const auto creatorsForLog = additionalCreators.isEmpty()
                                  ? QStringLiteral("none")
                                  : additionalCreators.join(QStringLiteral(", "));
    komai::logging::ui()->info(
      "Requesting room upgrade for '{}' to version '{}' (additional creators: {})",
      roomId.toStdString(),
      newVersion.toStdString(),
      creatorsForLog.toStdString());

    emit roomUpgradeStarted(roomId);

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, newVersion, additionalCreators]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          auto replacement = komai::MatrixBackendRuntimeService::upgradeRoom(
            context, handleId, roomId, newVersion, additionalCreators, &error);
          return std::make_pair(std::move(replacement), error);
      },
      [roomId](TimelineViewManager *self,
               const std::pair<std::optional<QString>, QString> &outcome) {
          const auto &[replacement, error] = outcome;
          auto *mw                         = MainWindow::instance();
          if (!replacement.has_value()) {
              komai::logging::ui()->warn(
                "Failed to upgrade room '{}': {}", roomId.toStdString(), error.toStdString());
              if (mw)
                  mw->showNotification(self->tr("Failed to upgrade room: %1").arg(error));
              return;
          }

          const auto &newRoomId = *replacement;
          komai::logging::ui()->info("Room '{}' upgraded; replacement room is '{}'",
                                     roomId.toStdString(),
                                     newRoomId.toStdString());

          // Switch to the successor.  setCurrentRoom() handles the case
          // where sync hasn't yet delivered the new room — the switch is
          // queued via pendingCurrentRoomId_ and applied when it appears.
          if (auto *rooms = FilteredRoomlistModel::instance())
              rooms->setCurrentRoom(newRoomId);

          if (mw) {
              // The deferred-switch path picks the new room up once sync
              // surfaces it in the room list (same flow as createRoom).
              mw->showNotification(
                self->tr("Room upgraded. Switching to the new room when it appears…"));
          }
      });
}

void
TimelineViewManager::openInviteUsers(QString roomId)
{
    if (!roomId.startsWith('!'))
        return;

    const auto preview  = rooms_ ? rooms_->getRoomPreviewById(roomId) : RoomPreview{};
    const auto roomName = preview.roomName().trimmed().isEmpty() ? roomId : preview.roomName();
    auto *model         = new InviteesModel{roomName};
    connect(model, &InviteesModel::accept, this, [this, model, roomId]() {
        emit inviteUsers(roomId, model->mxids());
    });
    QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
    emit openInviteUsersDialog(model);
}

void
TimelineViewManager::openGlobalUserProfile(QString userId)
{
    if (!userId.startsWith('@'))
        return;

    auto *profile = new UserProfile{QString{}, userId, this};
    QQmlEngine::setObjectOwnership(profile, QQmlEngine::JavaScriptOwnership);
    emit openProfile(profile);
}

void
TimelineViewManager::openRoomUserProfile(QString roomId, QString userId)
{
    if (!roomId.startsWith('!') || !userId.startsWith('@'))
        return;

    const auto preview  = rooms_ ? rooms_->getRoomPreviewById(roomId) : RoomPreview{};
    const auto roomName = preview.roomName().trimmed().isEmpty() ? roomId : preview.roomName();
    auto *profile       = new UserProfile{roomId, userId, this, roomName, preview.roomAvatarUrl()};
    QQmlEngine::setObjectOwnership(profile, QQmlEngine::JavaScriptOwnership);
    emit openProfile(profile);
}

UserProfile *
TimelineViewManager::getGlobalUserProfile(QString userId)
{
    auto *profile = new UserProfile{QString{}, userId, this};
    QQmlEngine::setObjectOwnership(profile, QQmlEngine::JavaScriptOwnership);
    return profile;
}

void
TimelineViewManager::setVideoCallItem()
{
    WebRTCSession::instance().setVideoItem(
      MainWindow::instance()->rootObject()->findChild<QQuickItem *>(
        QStringLiteral("videoCallItem")));
}

void
TimelineViewManager::showEvent(const QString &room_id, const QString &event_id)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow)
        return;

    const auto trimmedRoomId  = room_id.trimmed();
    const auto trimmedEventId = event_id.trimmed();

    if (auto *existingWindow = mainWindow->windowForRoom(trimmedRoomId)) {
        existingWindow->setVisible(true);
        existingWindow->raise();
        existingWindow->requestActivate();
    } else {
        rooms_->setCurrentRoom(trimmedRoomId);
        mainWindow->setVisible(true);
        mainWindow->raise();
        mainWindow->requestActivate();
        komai::logging::ui()->info("Activated room {}", trimmedRoomId.toStdString());
    }

    queueActiveMatrixPendingJump(trimmedRoomId, trimmedEventId);
}

void
TimelineViewManager::queueActiveMatrixPendingJump(const QString &roomId, const QString &eventId)
{
    const auto trimmedRoomId  = roomId.trimmed();
    const auto trimmedEventId = eventId.trimmed();
    if (trimmedRoomId.isEmpty() || trimmedEventId.isEmpty())
        return;

    const auto changed = matrixTimelinePendingJumpRoomId_ != trimmedRoomId ||
                         matrixTimelinePendingJumpEventId_ != trimmedEventId ||
                         matrixTimelinePendingJumpPaginationAttempts_ != 0 ||
                         matrixTimelinePendingJumpAwaitingSnapshot_ ||
                         matrixTimelinePendingJumpExhaustedLogged_;

    matrixTimelinePendingJumpRoomId_             = trimmedRoomId;
    matrixTimelinePendingJumpEventId_            = trimmedEventId;
    matrixTimelinePendingJumpPaginationAttempts_ = 0;
    matrixTimelinePendingJumpAwaitingSnapshot_   = false;
    matrixTimelinePendingJumpExhaustedLogged_    = false;

    komai::logging::ui()->info("Queued matrix-sdk pending event jump room='{}' event='{}'",
                               trimmedRoomId.toStdString(),
                               trimmedEventId.toStdString());

    if (changed)
        emit matrixTimelineStateChanged();
}

bool
TimelineViewManager::resolveActiveMatrixPendingJump()
{
    const auto pendingRoomId  = matrixTimelinePendingJumpRoomId_.trimmed();
    const auto pendingEventId = matrixTimelinePendingJumpEventId_.trimmed();
    if (pendingRoomId.isEmpty() || pendingEventId.isEmpty() || !matrixTimelineModel_ ||
        activeMatrixTimelineRoomId_ != pendingRoomId) {
        return false;
    }

    if (matrixTimelineModel_->rowForEventId(pendingEventId) >= 0)
        return true;

    if (matrixTimelineLoading_ || matrixTimelinePendingJumpAwaitingSnapshot_)
        return false;

    const auto hiddenCount = matrixTimelineModel_->hiddenCount();
    if (hiddenCount > 0) {
        const auto revealCount = std::min(hiddenCount, kMatrixPendingJumpPageSize);
        komai::logging::ui()->info(
          "Revealing {} locally-hidden matrix-sdk timeline items while resolving event jump "
          "room='{}' event='{}'",
          revealCount,
          pendingRoomId.toStdString(),
          pendingEventId.toStdString());
        matrixTimelineModel_->revealOlderItems(revealCount);
        // Revealing is model-local — no backend snapshot follows to re-run
        // the QML resolver, so queue the state-change notification ourselves.
        QMetaObject::invokeMethod(
          this, [this]() { emit matrixTimelineStateChanged(); }, Qt::QueuedConnection);
        return false;
    }

    if (matrixTimelinePendingJumpPaginationAttempts_ >= kMatrixPendingJumpMaxPageRequests) {
        if (!matrixTimelinePendingJumpExhaustedLogged_) {
            matrixTimelinePendingJumpExhaustedLogged_ = true;
            komai::logging::ui()->warn(
              "Stopped auto-paginating matrix-sdk timeline while resolving event jump after {} "
              "requests (room='{}', event='{}')",
              matrixTimelinePendingJumpPaginationAttempts_,
              pendingRoomId.toStdString(),
              pendingEventId.toStdString());
        }
        return false;
    }

    matrixTimelinePendingJumpAwaitingSnapshot_ = true;
    matrixTimelinePendingJumpPaginationAttempts_++;

    komai::logging::ui()->info(
      "Paginating matrix-sdk timeline to resolve pending event jump room='{}' event='{}' "
      "attempt={}/{}",
      pendingRoomId.toStdString(),
      pendingEventId.toStdString(),
      matrixTimelinePendingJumpPaginationAttempts_,
      kMatrixPendingJumpMaxPageRequests);

    if (paginateActiveMatrixTimelineBackwards(kMatrixPendingJumpPageSize))
        return false;

    matrixTimelinePendingJumpAwaitingSnapshot_ = false;
    return false;
}

void
TimelineViewManager::clearActiveMatrixPendingJump(const QString &eventId)
{
    const auto trimmedEventId = eventId.trimmed();
    if (!trimmedEventId.isEmpty() && trimmedEventId != matrixTimelinePendingJumpEventId_)
        return;

    if (matrixTimelinePendingJumpRoomId_.isEmpty() && matrixTimelinePendingJumpEventId_.isEmpty() &&
        matrixTimelinePendingJumpPaginationAttempts_ == 0 &&
        !matrixTimelinePendingJumpAwaitingSnapshot_ && !matrixTimelinePendingJumpExhaustedLogged_) {
        return;
    }

    matrixTimelinePendingJumpRoomId_.clear();
    matrixTimelinePendingJumpEventId_.clear();
    matrixTimelinePendingJumpPaginationAttempts_ = 0;
    matrixTimelinePendingJumpAwaitingSnapshot_   = false;
    matrixTimelinePendingJumpExhaustedLogged_    = false;
    emit matrixTimelineStateChanged();
}

void
TimelineViewManager::updateReadReceipts(const QString &room_id,
                                        const std::vector<QString> &event_ids)
{
    if (room_id != activeMatrixTimelineRoomId_ || event_ids.empty())
        return;

    markActiveMatrixTimelineEventAsRead(event_ids.back());
}

void
TimelineViewManager::initializeRoomlist()
{
    rooms_->initializeRooms();

    const auto *mainWindow = MainWindow::instance();
    if (!(mainWindow && mainWindow->matrixBackendHandleId() != 0))
        scheduleMatrixSidebarRefresh();
}

void
TimelineViewManager::queueReply(const QString &roomid,
                                const QString &repliedToEvent,
                                const QString &replyBody)
{
    rooms_->setCurrentRoom(roomid);

    QString senderId;
    QString senderDisplayName;
    QString body = replyBody.trimmed();

    if (roomid == activeMatrixTimelineRoomId_ && matrixTimelineModel_) {
        if (const auto item = matrixTimelineModel_->itemByEventId(repliedToEvent.trimmed())) {
            senderId          = item->senderId;
            senderDisplayName = item->senderDisplayName;
            if (body.isEmpty())
                body = item->body;
        }
    }

    if (setActiveMatrixReplyState(repliedToEvent, senderId, senderDisplayName, body))
        emit matrixTimelineStateChanged();

    focusMessageInput();
}

void
TimelineViewManager::ignoreUser(const QString &userId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow)
        return;

    const auto handleId = mainWindow->matrixBackendHandleId();
    if (handleId == 0)
        return;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, userId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok =
            komai::MatrixBackendRuntimeService::ignoreUser(context, handleId, userId, &error);
          return std::make_pair(ok, error);
      },
      [userId](TimelineViewManager *, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (ok)
              return;
          if (auto *mw = MainWindow::instance()) {
              mw->showNotification(
                TimelineViewManager::tr("Failed to ignore user %1: %2").arg(userId, error));
          }
      });
}

void
TimelineViewManager::focusMessageInput()
{
    emit focusInput();
}

void
TimelineViewManager::requestEscape()
{
    emit escapeRequested();
}
