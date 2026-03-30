// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <QQuickItem>
#include <QTimer>

#include "RoomlistModel.h"
#include "logging/Logging.h"
#include "models/InviteesModel.h"
#include "models/MemberList.h"
#include "timeline/CommunitiesModel.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "ui/MainWindow.h"
#include "ui/RoomSettings.h"
#include "ui/UserProfile.h"
#include "voip/WebRTCSession.h"

namespace {
constexpr auto kMatrixPendingJumpPageSize        = 50;
constexpr auto kMatrixPendingJumpMaxPageRequests = 8;
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
TimelineViewManager::openInviteUsers(QString roomId)
{
    if (!roomId.startsWith('!'))
        return;

    auto *model = new InviteesModel{nullptr};
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
        nhlog::ui()->info("Activated room {}", trimmedRoomId.toStdString());
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

    nhlog::ui()->info("Queued matrix-sdk pending event jump room='{}' event='{}'",
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
        nhlog::ui()->info(
          "Revealing {} locally-hidden matrix-sdk timeline items while resolving event jump "
          "room='{}' event='{}'",
          revealCount,
          pendingRoomId.toStdString(),
          pendingEventId.toStdString());
        matrixTimelineModel_->revealOlderItems(revealCount);
        return false;
    }

    if (matrixTimelinePendingJumpPaginationAttempts_ >= kMatrixPendingJumpMaxPageRequests) {
        if (!matrixTimelinePendingJumpExhaustedLogged_) {
            matrixTimelinePendingJumpExhaustedLogged_ = true;
            nhlog::ui()->warn(
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

    nhlog::ui()->info(
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
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallInvite &callInvite)
{
    Q_UNUSED(roomid);
    Q_UNUSED(callInvite);
    nhlog::ui()->warn("Legacy call-event send path is not migrated to matrix-sdk yet");
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallCandidates &callCandidates)
{
    Q_UNUSED(roomid);
    Q_UNUSED(callCandidates);
    nhlog::ui()->warn("Legacy call-event send path is not migrated to matrix-sdk yet");
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallAnswer &callAnswer)
{
    Q_UNUSED(roomid);
    Q_UNUSED(callAnswer);
    nhlog::ui()->warn("Legacy call-event send path is not migrated to matrix-sdk yet");
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallHangUp &callHangUp)
{
    Q_UNUSED(roomid);
    Q_UNUSED(callHangUp);
    nhlog::ui()->warn("Legacy call-event send path is not migrated to matrix-sdk yet");
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallSelectAnswer &callSelectAnswer)
{
    Q_UNUSED(roomid);
    Q_UNUSED(callSelectAnswer);
    nhlog::ui()->warn("Legacy call-event send path is not migrated to matrix-sdk yet");
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallReject &callReject)
{
    Q_UNUSED(roomid);
    Q_UNUSED(callReject);
    nhlog::ui()->warn("Legacy call-event send path is not migrated to matrix-sdk yet");
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallNegotiate &callNegotiate)
{
    Q_UNUSED(roomid);
    Q_UNUSED(callNegotiate);
    nhlog::ui()->warn("Legacy call-event send path is not migrated to matrix-sdk yet");
}

void
TimelineViewManager::focusMessageInput()
{
    emit focusInput();
}
