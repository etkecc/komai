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
TimelineViewManager::openRoomMembers(QObject *room)
{
    Q_UNUSED(room);
    if (auto *mainWindow = MainWindow::instance())
        mainWindow->showNotification(tr("Legacy room-member opening is not migrated yet."));
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

    if (auto *existingWindow = mainWindow->windowForRoom(room_id)) {
        existingWindow->setVisible(true);
        existingWindow->raise();
        existingWindow->requestActivate();
    } else {
        rooms_->setCurrentRoom(room_id);
        mainWindow->setVisible(true);
        mainWindow->raise();
        mainWindow->requestActivate();
        nhlog::ui()->info("Activated room {}", room_id.toStdString());
    }

    if (room_id == activeMatrixTimelineRoomId_ && !event_id.trimmed().isEmpty()) {
        nhlog::ui()->warn("Jump-to-event for matrix-sdk rooms is not migrated yet (room='{}', "
                          "event='{}')",
                          room_id.toStdString(),
                          event_id.toStdString());
    }
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
