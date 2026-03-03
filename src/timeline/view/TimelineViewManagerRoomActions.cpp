// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <QQuickItem>

#include "InviteesModel.h"
#include "Logging.h"
#include "MainWindow.h"
#include "RoomlistModel.h"
#include "TimelineModel.h"
#include "models/MemberList.h"
#include "timeline/CommunitiesModel.h"
#include "ui/RoomSettings.h"
#include "ui/UserProfile.h"
#include "voip/WebRTCSession.h"

void
TimelineViewManager::openRoomMembers(TimelineModel *room)
{
    if (room)
        openRoomInfo(room->roomId(), QStringLiteral("members"));
}

void
TimelineViewManager::openRoomSettings(QString room_id)
{
    openRoomInfo(room_id, QStringLiteral("settings"));
}

void
TimelineViewManager::openRoomInfo(const QString &roomId, const QString &initialTab)
{
    auto room = rooms_->getRoomById(roomId);
    if (!room)
        return;

    auto *settings = new RoomSettings(roomId);
    connect(
      room.data(), &TimelineModel::roomAvatarUrlChanged, settings, &RoomSettings::avatarChanged);
    QQmlEngine::setObjectOwnership(settings, QQmlEngine::JavaScriptOwnership);

    auto *memberList = new MemberList(roomId);
    QQmlEngine::setObjectOwnership(memberList, QQmlEngine::JavaScriptOwnership);

    emit openRoomInfoDialog(settings, memberList, room.data(), initialTab);
}

void
TimelineViewManager::openInviteUsers(QString roomId)
{
    if (!roomId.startsWith('!'))
        return;

    InviteesModel *model = new InviteesModel{rooms_->getRoomById(roomId).data()};
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

    UserProfile *profile = new UserProfile{QString{}, userId, this};
    QQmlEngine::setObjectOwnership(profile, QQmlEngine::JavaScriptOwnership);
    emit openProfile(profile);
}

UserProfile *
TimelineViewManager::getGlobalUserProfile(QString userId)
{
    UserProfile *profile = new UserProfile{QString{}, userId, this};
    QQmlEngine::setObjectOwnership(profile, QQmlEngine::JavaScriptOwnership);
    return (profile);
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
    if (auto room = rooms_->getRoomById(room_id)) {
        auto exWin = MainWindow::instance()->windowForRoom(room_id);
        if (exWin) {
            exWin->setVisible(true);
            exWin->raise();
            exWin->requestActivate();
        } else {
            rooms_->setCurrentRoom(room_id);
            MainWindow::instance()->setVisible(true);
            MainWindow::instance()->raise();
            MainWindow::instance()->requestActivate();
            nhlog::ui()->info("Activated room {}", room_id.toStdString());
        }

        room->showEvent(event_id);
    }
}

void
TimelineViewManager::updateReadReceipts(const QString &room_id,
                                        const std::vector<QString> &event_ids)
{
    if (auto room = rooms_->getMaterializedRoomById(room_id)) {
        room->markEventsAsRead(event_ids);
    }
}

void
TimelineViewManager::receivedSessionKey(const std::string &room_id, const std::string &session_id)
{
    if (auto room = rooms_->getMaterializedRoomById(QString::fromStdString(room_id))) {
        room->receivedSessionKey(session_id);
    }
}

void
TimelineViewManager::initializeRoomlist()
{
    rooms_->initializeRooms();
    communities_->initializeSidebar();
}

void
TimelineViewManager::queueReply(const QString &roomid,
                                const QString &repliedToEvent,
                                const QString &replyBody)
{
    if (auto room = rooms_->getRoomById(roomid)) {
        room->setReply(repliedToEvent);
        room->input()->message(replyBody);
    }
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallInvite &callInvite)
{
    if (auto room = rooms_->getRoomById(roomid))
        room->sendMessageEvent(callInvite, mtx::events::EventType::CallInvite);
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallCandidates &callCandidates)
{
    if (auto room = rooms_->getRoomById(roomid))
        room->sendMessageEvent(callCandidates, mtx::events::EventType::CallCandidates);
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallAnswer &callAnswer)
{
    if (auto room = rooms_->getRoomById(roomid))
        room->sendMessageEvent(callAnswer, mtx::events::EventType::CallAnswer);
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallHangUp &callHangUp)
{
    if (auto room = rooms_->getRoomById(roomid))
        room->sendMessageEvent(callHangUp, mtx::events::EventType::CallHangUp);
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallSelectAnswer &callSelectAnswer)
{
    if (auto room = rooms_->getRoomById(roomid))
        room->sendMessageEvent(callSelectAnswer, mtx::events::EventType::CallSelectAnswer);
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallReject &callReject)
{
    if (auto room = rooms_->getRoomById(roomid))
        room->sendMessageEvent(callReject, mtx::events::EventType::CallReject);
}

void
TimelineViewManager::queueCallMessage(const QString &roomid,
                                      const mtx::events::voip::CallNegotiate &callNegotiate)
{
    if (auto room = rooms_->getRoomById(roomid))
        room->sendMessageEvent(callNegotiate, mtx::events::EventType::CallNegotiate);
}

void
TimelineViewManager::focusMessageInput()
{
    emit focusInput();
}
