// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>

#include <QAudioOutput>
#include <QGuiApplication>
#include <QMetaObject>
#include <QThread>
#include <QUrl>

#include "CallDevices.h"
#include "CallManager.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

#include "mtx/responses/turn_server.hpp"
#include "voip/ScreenCastPortal.h"
#include "voip/WebRTCSession.h"

/*
 * Select Answer when one instance of the client supports v0
 */

using namespace mtx::events;
using namespace mtx::events::voip;

using webrtc::CallType;
using webrtc::ScreenShareType;
//! Session Description Object
typedef RTCSessionDescriptionInit SDO;

namespace {
std::vector<std::string>
getTurnURIs(const mtx::responses::TurnServer &turnServer);
}

CallManager *
CallManager::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    // The instance has to exist before it is used. We cannot replace it.
    auto instance = ChatPage::instance()->callManager();
    Q_ASSERT(instance);

    // The engine has to have the same thread affinity as the singleton.
    Q_ASSERT(qmlEngine->thread() == instance->thread());

    // There can only be one engine accessing the singleton.
    static QJSEngine *s_engine = nullptr;
    if (s_engine)
        Q_ASSERT(qmlEngine == s_engine);
    else
        s_engine = qmlEngine;

    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

CallManager::CallManager(QObject *parent)
  : QObject(parent)
  , session_(WebRTCSession::instance())
  , turnServerTimer_(this)
{
#ifdef GSTREAMER_AVAILABLE
    const auto addScreenShareType = [this](ScreenShareType type) {
        if (std::find(screenShareTypes_.begin(), screenShareTypes_.end(), type) ==
            screenShareTypes_.end()) {
            screenShareTypes_.push_back(type);
        }
    };

    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        screenShareType_ = ScreenShareType::D3D11;
        addScreenShareType(ScreenShareType::D3D11);
    } else {
        const bool isWayland = QGuiApplication::platformName() == QStringLiteral("wayland");
        if (isWayland) {
            screenShareType_ = ScreenShareType::XDP;
            addScreenShareType(ScreenShareType::XDP);
        }

        if (std::getenv("DISPLAY")) {
            addScreenShareType(ScreenShareType::X11);
            if (!isWayland)
                screenShareType_ = ScreenShareType::X11;
        }

        if (!isWayland) {
            // Keep XDP available as an option on non-Windows platforms.
            addScreenShareType(ScreenShareType::XDP);
        }

        if (screenShareTypes_.empty()) {
            screenShareType_ = ScreenShareType::XDP;
            addScreenShareType(ScreenShareType::XDP);
        }
    }
#endif

    connect(
      &session_,
      &WebRTCSession::offerCreated,
      this,
      [this](const std::string &sdp, const std::vector<CallCandidates::Candidate> &candidates) {
          nhlog::ui()->debug("WebRTC: call id: {} - sending offer", callid_);
          emit newMessage(roomid_,
                          CallInvite{callid_,
                                     partyid_,
                                     SDO{sdp, SDO::Type::Offer},
                                     callPartyVersion_,
                                     timeoutms_,
                                     invitee_});
          emit newMessage(roomid_,
                          CallCandidates{callid_, partyid_, candidates, callPartyVersion_});
          std::string callid(callid_);
          QTimer::singleShot(timeoutms_, this, [this, callid]() {
              if (session_.state() == webrtc::State::OFFERSENT && callid == callid_) {
                  hangUp(CallHangUp::Reason::InviteTimeOut);
                  emit ChatPage::instance()->showNotification(
                    QStringLiteral("The remote side failed to pick up."));
              }
          });
      });

    connect(
      &session_,
      &WebRTCSession::answerCreated,
      this,
      [this](const std::string &sdp, const std::vector<CallCandidates::Candidate> &candidates) {
          nhlog::ui()->debug("WebRTC: call id: {} - sending answer", callid_);
          emit newMessage(
            roomid_, CallAnswer{callid_, partyid_, callPartyVersion_, SDO{sdp, SDO::Type::Answer}});
          emit newMessage(roomid_,
                          CallCandidates{callid_, partyid_, candidates, callPartyVersion_});
      });

    connect(&session_,
            &WebRTCSession::newICECandidate,
            this,
            [this](const CallCandidates::Candidate &candidate) {
                nhlog::ui()->debug("WebRTC: call id: {} - sending ice candidate", callid_);
                emit newMessage(roomid_,
                                CallCandidates{callid_, partyid_, {candidate}, callPartyVersion_});
            });

    connect(&turnServerTimer_, &QTimer::timeout, this, &CallManager::retrieveTurnServer);

    connect(
      this, &CallManager::turnServerRetrieved, this, [this](const mtx::responses::TurnServer &res) {
          nhlog::net()->info("TURN server(s) retrieved from homeserver:");
          nhlog::net()->info("username: {}", res.username);
          nhlog::net()->info("ttl: {} seconds", res.ttl);
          for (const auto &u : res.uris)
              nhlog::net()->info("uri: {}", u);

          // Request new credentials close to expiry
          // See https://tools.ietf.org/html/draft-uberti-behave-turn-rest-00
          turnURIs_    = getTurnURIs(res);
          uint32_t ttl = std::max(res.ttl, std::uint32_t{3600});
          if (res.ttl < 3600)
              nhlog::net()->warn("Setting ttl to 1 hour");
          turnServerTimer_.setInterval(std::chrono::seconds(ttl) * 10 / 9);
      });

    connect(&session_, &WebRTCSession::stateChanged, this, [this](webrtc::State state) {
        switch (state) {
        case webrtc::State::DISCONNECTED:
            playRingtone(QUrl(QStringLiteral("qrc:/media/media/callend.ogg")), false);
            clear();
            break;
        case webrtc::State::ICEFAILED: {
            QString error(QStringLiteral("Call connection failed."));
            if (turnURIs_.empty())
                error += QLatin1String(" Your homeserver has no configured TURN server.");
            emit ChatPage::instance()->showNotification(error);
            hangUp(CallHangUp::Reason::ICEFailed);
            break;
        }
        default:
            break;
        }
        emit newCallState();
    });

    connect(
      &CallDevices::instance(), &CallDevices::devicesChanged, this, &CallManager::devicesChanged);

#ifdef GSTREAMER_AVAILABLE
    connect(&ScreenCastPortal::instance(),
            &ScreenCastPortal::readyChanged,
            this,
            &CallManager::screenShareChanged);
#endif
}

QMediaPlayer *
CallManager::ensurePlayerInitialized()
{
    std::call_once(playerInitOnce_, [this]() {
        player_ = std::make_unique<QMediaPlayer>(this);

        auto audioOutput = new QAudioOutput(player_.get());
        player_->setAudioOutput(audioOutput);

        connect(player_.get(),
                &QMediaPlayer::mediaStatusChanged,
                this,
                [](QMediaPlayer::MediaStatus status) {
                    nhlog::ui()->debug(
                      "WebRTC: ringtone status {}",
                      QMetaEnum::fromType<QMediaPlayer::MediaStatus>().valueToKey(status));
                });

        connect(player_.get(),
                &QMediaPlayer::errorOccurred,
                this,
                [this](QMediaPlayer::Error error, QString errorString) {
                    stopRingtone();
                    switch (error) {
                    case QMediaPlayer::FormatError:
                    case QMediaPlayer::ResourceError:
                        nhlog::ui()->error("WebRTC: valid ringtone file not found");
                        break;
                    case QMediaPlayer::AccessDeniedError:
                        nhlog::ui()->error("WebRTC: access to ringtone file denied");
                        break;
                    default:
                        nhlog::ui()->error("WebRTC: unable to play ringtone, {}",
                                           errorString.toStdString());
                        break;
                    }
                });
    });

    return player_.get();
}

void
CallManager::sendInvite(const QString &roomid, CallType callType, unsigned int windowIndex)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
    if (isOnCall() || isOnCallOnOtherDevice()) {
        if (isOnCallOnOtherDevice_ != "")
            emit ChatPage::instance()->showNotification(
              QStringLiteral("User is already in a call"));
        return;
    }

    auto roomInfo = cache::singleRoomInfo(roomid.toStdString());

    std::string errorMessage;

    callType_ = callType;
    roomid_   = roomid;
    generateCallID();
    std::vector<RoomMember> members(cache::getMembers(roomid.toStdString()));
    const RoomMember *callee;
    if (roomInfo.member_count == 1)
        callee = &members.front();
    else if (roomInfo.member_count == 2)
        callee = members.front().user_id == utils::localUser() ? &members.back() : &members.front();
    else {
        emit ChatPage::instance()->showNotification(
          QStringLiteral("Calls are limited to rooms with less than two members"));
        return;
    }

#ifdef GSTREAMER_AVAILABLE
    if (callType == CallType::SCREEN) {
        if (screenShareType_ == ScreenShareType::X11 ||
            screenShareType_ == ScreenShareType::D3D11) {
            if (windows_.empty() || windowIndex >= windows_.size()) {
                nhlog::ui()->error("WebRTC: window index out of range");
                return;
            }
        } else {
            ScreenCastPortal &sc_portal = ScreenCastPortal::instance();
            if (sc_portal.getStream() == nullptr) {
                nhlog::ui()->error("xdg-desktop-portal stream not started");
                return;
            }
        }
    }
#endif

    if (haveCallInvite_) {
        nhlog::ui()->debug("WebRTC: Discarding outbound call for inbound call. "
                           "localUser is polite party");
        if (callParty_ == callee->user_id) {
            if (callType == callType_)
                acceptInvite();
            else {
                emit ChatPage::instance()->showNotification(
                  QStringLiteral("Can't place call. Call types do not match"));
                emit newMessage(
                  roomid_,
                  CallHangUp{callid_, partyid_, callPartyVersion_, CallHangUp::Reason::UserBusy});
            }
        } else {
            emit ChatPage::instance()->showNotification(
              QStringLiteral("Already on a call with a different user"));
            emit newMessage(
              roomid_,
              CallHangUp{callid_, partyid_, callPartyVersion_, CallHangUp::Reason::UserBusy});
        }
        return;
    }

    session_.setTurnServers(turnURIs_);
    std::string strCallType =
      callType_ == CallType::VOICE ? "voice" : (callType_ == CallType::VIDEO ? "video" : "screen");

    nhlog::ui()->debug("WebRTC: call id: {} - creating {} invite", callid_, strCallType);
    callParty_            = callee->user_id;
    callPartyDisplayName_ = callee->display_name.isEmpty() ? callee->user_id : callee->display_name;
    callPartyAvatarUrl_   = QString::fromStdString(roomInfo.avatar_url);
    invitee_              = callParty_.toStdString();
    emit newInviteState();
    playRingtone(QUrl(QStringLiteral("qrc:/media/media/ringback.ogg")), true);

    uint32_t shareWindowId =
      callType == CallType::SCREEN &&
          (screenShareType_ == ScreenShareType::X11 || screenShareType_ == ScreenShareType::D3D11)
        ? windows_[windowIndex].second
        : 0;
    if (!session_.createOffer(callType, screenShareType_, shareWindowId)) {
        emit ChatPage::instance()->showNotification(QStringLiteral("Problem setting up call."));
        endCall();
    }
}

namespace {
std::string
callHangUpReasonString(CallHangUp::Reason reason)
{
    switch (reason) {
    case CallHangUp::Reason::ICEFailed:
        return "ICE failed";
    case CallHangUp::Reason::InviteTimeOut:
        return "Invite time out";
    case CallHangUp::Reason::ICETimeOut:
        return "ICE time out";
    case CallHangUp::Reason::UserHangUp:
        return "User hung up";
    case CallHangUp::Reason::UserMediaFailed:
        return "User media failed";
    case CallHangUp::Reason::UserBusy:
        return "User busy";
    case CallHangUp::Reason::UnknownError:
        return "Unknown error";
    default:
        return "User";
    }
}
} // namespace

void
CallManager::hangUp(CallHangUp::Reason reason)
{
    if (!callid_.empty()) {
        nhlog::ui()->debug(
          "WebRTC: call id: {} - hanging up ({})", callid_, callHangUpReasonString(reason));
        emit newMessage(roomid_, CallHangUp{callid_, partyid_, callPartyVersion_, reason});
        endCall();
    }
}

void
CallManager::syncEvent(const mtx::events::collections::TimelineEvents &event)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    if (handleEvent<CallInvite>(event) || handleEvent<CallCandidates>(event) ||
        handleEvent<CallNegotiate>(event) || handleEvent<CallSelectAnswer>(event) ||
        handleEvent<CallAnswer>(event) || handleEvent<CallReject>(event) ||
        handleEvent<CallHangUp>(event))
        return;
#else
    (void)event;
#endif
}

template<typename T>
bool
CallManager::handleEvent(const mtx::events::collections::TimelineEvents &event)
{
    if (std::holds_alternative<RoomEvent<T>>(event)) {
        handleEvent(std::get<RoomEvent<T>>(event));
        return true;
    }
    return false;
}

void
CallManager::handleEvent(const RoomEvent<CallInvite> &callInviteEvent)
{
    const char video[]     = "m=video";
    const std::string &sdp = callInviteEvent.content.offer.sdp;
    bool isVideo           = std::search(sdp.cbegin(),
                               sdp.cend(),
                               std::cbegin(video),
                               std::cend(video) - 1,
                               [](unsigned char c1, unsigned char c2) {
                                   return std::tolower(c1) == std::tolower(c2);
                               }) != sdp.cend();
    nhlog::ui()->debug("WebRTC: call id: {} - incoming {} CallInvite from ({},{}) ",
                       callInviteEvent.content.call_id,
                       (isVideo ? "video" : "voice"),
                       callInviteEvent.sender,
                       callInviteEvent.content.party_id);

    if (callInviteEvent.content.call_id.empty())
        return;

    if (callInviteEvent.sender == utils::localUser().toStdString()) {
        if (callInviteEvent.content.party_id == partyid_)
            return;
        else {
            if (callInviteEvent.content.invitee != utils::localUser().toStdString()) {
                isOnCallOnOtherDevice_ = callInviteEvent.content.call_id;
                emit newCallDeviceState();
                nhlog::ui()->debug("WebRTC: User is on call on other device.");
                return;
            }
        }
    }

    auto roomInfo     = cache::singleRoomInfo(callInviteEvent.room_id);
    callPartyVersion_ = callInviteEvent.content.version;

    const QString &ringtone = UserSettings::instance()->callsAudioRingtone();
    bool sharesRoom         = true;

    std::vector<RoomMember> members(cache::getMembers(callInviteEvent.room_id));
    const RoomMember &caller =
      *std::find_if(members.begin(), members.end(), [&](RoomMember member) {
          return member.user_id.toStdString() == callInviteEvent.sender;
      });
    if (isOnCall() || isOnCallOnOtherDevice()) {
        if (isOnCallOnOtherDevice_ != "")
            return;
        if (callParty_.toStdString() == callInviteEvent.sender) {
            if (session_.state() == webrtc::State::OFFERSENT) {
                if (callid_ > callInviteEvent.content.call_id) {
                    endCall();
                    callParty_ = caller.user_id;
                    callPartyDisplayName_ =
                      caller.display_name.isEmpty() ? caller.user_id : caller.display_name;
                    callPartyAvatarUrl_ = QString::fromStdString(roomInfo.avatar_url);

                    roomid_ = QString::fromStdString(callInviteEvent.room_id);
                    callid_ = callInviteEvent.content.call_id;
                    remoteICECandidates_.clear();
                    haveCallInvite_ = true;
                    callType_       = isVideo ? CallType::VIDEO : CallType::VOICE;
                    inviteSDP_      = callInviteEvent.content.offer.sdp;
                    emit newInviteState();
                    acceptInvite();
                }
                return;
            } else if (session_.state() < webrtc::State::OFFERSENT)
                endCall();
            else
                return;
        } else
            return;
    }

    if (callPartyVersion_ == "0") {
        if (roomInfo.member_count != 2) {
            emit newMessage(QString::fromStdString(callInviteEvent.room_id),
                            CallHangUp{callInviteEvent.content.call_id,
                                       partyid_,
                                       callPartyVersion_,
                                       CallHangUp::Reason::InviteTimeOut});
            return;
        }
    } else {
        if (caller.user_id == utils::localUser() &&
            callInviteEvent.content.party_id == partyid_) // remote echo
            return;

        if (roomInfo.member_count == 2 || // general call in room with two members
            (roomInfo.member_count == 1 &&
             partyid_ !=
               callInviteEvent.content.party_id) ||  // self call, ring if not the same party_id
            callInviteEvent.content.invitee == "" || // empty, meant for everyone
            callInviteEvent.content.invitee ==
              utils::localUser().toStdString()) // meant specifically for local user
        {
            if (roomInfo.member_count > 2) {
                // check if shares room
                sharesRoom = checkSharesRoom(QString::fromStdString(callInviteEvent.room_id),
                                             callInviteEvent.content.invitee);
            }
        } else {
            emit newMessage(QString::fromStdString(callInviteEvent.room_id),
                            CallHangUp{callInviteEvent.content.call_id,
                                       partyid_,
                                       callPartyVersion_,
                                       CallHangUp::Reason::InviteTimeOut});
            return;
        }
    }

    // ring if not mute or does not have direct message room
    if (ringtone != QLatin1String("Mute") && sharesRoom)
        playRingtone(ringtone == QLatin1String("Default")
                       ? QUrl(QStringLiteral("qrc:/media/media/ring.ogg"))
                       : QUrl::fromLocalFile(ringtone),
                     true);

    callParty_            = caller.user_id;
    callPartyDisplayName_ = caller.display_name.isEmpty() ? caller.user_id : caller.display_name;
    callPartyAvatarUrl_   = QString::fromStdString(roomInfo.avatar_url);

    roomid_ = QString::fromStdString(callInviteEvent.room_id);
    callid_ = callInviteEvent.content.call_id;
    remoteICECandidates_.clear();

    haveCallInvite_ = true;
    callType_       = isVideo ? CallType::VIDEO : CallType::VOICE;
    inviteSDP_      = callInviteEvent.content.offer.sdp;
    emit newInviteState();
}

void
CallManager::acceptInvite()
{
    // if call was accepted/rejected elsewhere and m.call.select_answer is
    // received before acceptInvite
    if (!haveCallInvite_)
        return;

    stopRingtone();
    std::string errorMessage;
    if (!session_.havePlugins(callType_ != CallType::VOICE,
                              callType_ == CallType::SCREEN,
                              screenShareType_,
                              &errorMessage)) {
        emit ChatPage::instance()->showNotification(QString::fromStdString(errorMessage));
        hangUp(CallHangUp::Reason::UserMediaFailed);
        return;
    }

    session_.setTurnServers(turnURIs_);
    if (!session_.acceptOffer(inviteSDP_)) {
        emit ChatPage::instance()->showNotification(QStringLiteral("Problem setting up call."));
        hangUp();
        return;
    }
    session_.acceptICECandidates(remoteICECandidates_);
    remoteICECandidates_.clear();
    haveCallInvite_ = false;
    emit newInviteState();
}

void
CallManager::rejectInvite()
{
    if (callPartyVersion_ == "0") {
        hangUp();
        // send m.call.reject after sending hangup as mentioned in MSC2746
        emit newMessage(roomid_, CallReject{callid_, partyid_, callPartyVersion_});
    }
    if (!callid_.empty()) {
        nhlog::ui()->debug("WebRTC: call id: {} - rejecting call", callid_);
        emit newMessage(roomid_, CallReject{callid_, partyid_, callPartyVersion_});
        endCall(false);
    }
}

void
CallManager::handleEvent(const RoomEvent<CallCandidates> &callCandidatesEvent)
{
    if (callCandidatesEvent.sender == utils::localUser().toStdString() &&
        callCandidatesEvent.content.party_id == partyid_)
        return;
    nhlog::ui()->debug("WebRTC: call id: {} - incoming CallCandidates from ({}, {})",
                       callCandidatesEvent.content.call_id,
                       callCandidatesEvent.sender,
                       callCandidatesEvent.content.party_id);

    if (callid_ == callCandidatesEvent.content.call_id) {
        if (isOnCall())
            session_.acceptICECandidates(callCandidatesEvent.content.candidates);
        else {
            // CallInvite has been received and we're awaiting localUser to accept or
            // reject the call
            for (const auto &c : callCandidatesEvent.content.candidates)
                remoteICECandidates_.push_back(c);
        }
    }
}

void
CallManager::handleEvent(const RoomEvent<CallAnswer> &callAnswerEvent)
{
    nhlog::ui()->debug("WebRTC: call id: {} - incoming CallAnswer from ({}, {})",
                       callAnswerEvent.content.call_id,
                       callAnswerEvent.sender,
                       callAnswerEvent.content.party_id);
    if (answerSelected_)
        return;

    if (callAnswerEvent.sender == utils::localUser().toStdString() &&
        callid_ == callAnswerEvent.content.call_id) {
        if (partyid_ == callAnswerEvent.content.party_id)
            return;

        if (!isOnCall()) {
            emit ChatPage::instance()->showNotification(
              QStringLiteral("Call answered on another device."));
            stopRingtone();
            haveCallInvite_ = false;
            if (callPartyVersion_ != "1") {
                isOnCallOnOtherDevice_ = callid_;
                emit newCallDeviceState();
            }
            emit newInviteState();
        }
        if (callParty_ != utils::localUser())
            return;
    }

    if (isOnCall() && callid_ == callAnswerEvent.content.call_id) {
        stopRingtone();
        if (!session_.acceptAnswer(callAnswerEvent.content.answer.sdp)) {
            emit ChatPage::instance()->showNotification(QStringLiteral("Problem setting up call."));
            hangUp();
        }
    }
    emit newMessage(
      roomid_,
      CallSelectAnswer{callid_, partyid_, callPartyVersion_, callAnswerEvent.content.party_id});
    selectedpartyid_ = callAnswerEvent.content.party_id;
    answerSelected_  = true;
}

void
CallManager::handleEvent(const RoomEvent<CallHangUp> &callHangUpEvent)
{
    nhlog::ui()->debug("WebRTC: call id: {} - incoming CallHangUp ({}) from ({}, {})",
                       callHangUpEvent.content.call_id,
                       callHangUpReasonString(callHangUpEvent.content.reason),
                       callHangUpEvent.sender,
                       callHangUpEvent.content.party_id);

    if (callid_ == callHangUpEvent.content.call_id ||
        isOnCallOnOtherDevice_ == callHangUpEvent.content.call_id)
        endCall();
}

void
CallManager::handleEvent(const RoomEvent<CallSelectAnswer> &callSelectAnswerEvent)
{
    nhlog::ui()->debug("WebRTC: call id: {} - incoming CallSelectAnswer from ({}, {})",
                       callSelectAnswerEvent.content.call_id,
                       callSelectAnswerEvent.sender,
                       callSelectAnswerEvent.content.party_id);
    if (callSelectAnswerEvent.sender == utils::localUser().toStdString()) {
        if (callSelectAnswerEvent.content.party_id != partyid_) {
            if (std::find(rejectCallPartyIDs_.begin(),
                          rejectCallPartyIDs_.begin(),
                          callSelectAnswerEvent.content.selected_party_id) !=
                rejectCallPartyIDs_.end())
                endCall();
            else {
                if (callSelectAnswerEvent.content.selected_party_id == partyid_)
                    return;
                nhlog::ui()->debug("WebRTC: call id: {} - user is on call with this user!",
                                   callSelectAnswerEvent.content.call_id);
                isOnCallOnOtherDevice_ = callSelectAnswerEvent.content.call_id;
                emit newCallDeviceState();
            }
        }
        return;
    } else if (callid_ == callSelectAnswerEvent.content.call_id) {
        if (callSelectAnswerEvent.content.selected_party_id != partyid_) {
            bool endAllCalls = false;
            if (std::find(rejectCallPartyIDs_.begin(),
                          rejectCallPartyIDs_.begin(),
                          callSelectAnswerEvent.content.selected_party_id) !=
                rejectCallPartyIDs_.end())
                endAllCalls = true;
            else {
                isOnCallOnOtherDevice_ = callid_;
                emit newCallDeviceState();
            }
            endCall(endAllCalls);
        } else if (session_.state() == webrtc::State::DISCONNECTED)
            endCall();
    }
}

void
CallManager::handleEvent(const RoomEvent<CallReject> &callRejectEvent)
{
    nhlog::ui()->debug("WebRTC: call id: {} - incoming CallReject from ({}, {})",
                       callRejectEvent.content.call_id,
                       callRejectEvent.sender,
                       callRejectEvent.content.party_id);
    if (answerSelected_)
        return;

    rejectCallPartyIDs_.push_back(callRejectEvent.content.party_id);
    // check remote echo
    if (callRejectEvent.sender == utils::localUser().toStdString()) {
        if (callRejectEvent.content.party_id != partyid_ && callParty_ != utils::localUser())
            emit ChatPage::instance()->showNotification(
              QStringLiteral("Call rejected on another device."));
        endCall();
        return;
    }

    if (callRejectEvent.content.call_id == callid_) {
        if (session_.state() == webrtc::State::OFFERSENT) {
            // only accept reject if webrtc is in OFFERSENT state, else call has been
            // accepted
            emit newMessage(
              roomid_,
              CallSelectAnswer{
                callid_, partyid_, callPartyVersion_, callRejectEvent.content.party_id});
            endCall();
        }
    }
}

void
CallManager::handleEvent(const RoomEvent<CallNegotiate> &callNegotiateEvent)
{
    nhlog::ui()->debug("WebRTC: call id: {} - incoming CallNegotiate from ({}, {})",
                       callNegotiateEvent.content.call_id,
                       callNegotiateEvent.sender,
                       callNegotiateEvent.content.party_id);

    std::string negotiationSDP_ = callNegotiateEvent.content.description.sdp;
    if (!session_.acceptNegotiation(negotiationSDP_)) {
        emit ChatPage::instance()->showNotification(QStringLiteral("Problem accepting new SDP"));
        hangUp();
        return;
    }
    session_.acceptICECandidates(remoteICECandidates_);
    remoteICECandidates_.clear();
}

bool
CallManager::checkSharesRoom(QString roomid, std::string invitee) const
{
    /*
        IMPLEMENTATION REQUIRED
        Check if room is shared to determine whether to ring or not.
        Called from handle callInvite event
    */
    if (roomid.toStdString() != "") {
        if (invitee == "") {
            // check all members
            return true;
        } else {
            return true;
            // check if invitee shares a direct room with local user
        }
        return true;
    }

    return true;
}

void
CallManager::toggleMicMute()
{
    session_.toggleMicMute();
    emit micMuteChanged();
}

bool
CallManager::callsSupported()
{
#ifdef GSTREAMER_AVAILABLE
    return true;
#else
    return false;
#endif
}

QStringList
CallManager::devices(bool isVideo) const
{
    QStringList ret;
    (void)CallDevices::instance().ensureInitialized();
    const QString &defaultDevice = isVideo ? UserSettings::instance()->callsDevicesCamera()
                                           : UserSettings::instance()->callsDevicesMicrophone();
    std::vector<std::string> devices =
      CallDevices::instance().names(isVideo, defaultDevice.toStdString());
    assert(devices.size() < std::numeric_limits<int>::max());
    ret.reserve(static_cast<int>(devices.size()));
    std::transform(devices.cbegin(), devices.cend(), std::back_inserter(ret), [](const auto &d) {
        return QString::fromStdString(d);
    });

    return ret;
}

void
CallManager::generateCallID()
{
    using namespace std::chrono;
    uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    callid_     = "c" + std::to_string(ms);
}

void
CallManager::clear(bool endAllCalls)
{
    closeScreenShare();
    roomid_.clear();
    callParty_.clear();
    callPartyDisplayName_.clear();
    callPartyAvatarUrl_.clear();
    callid_.clear();
    callType_       = CallType::VOICE;
    haveCallInvite_ = false;
    answerSelected_ = false;
    if (endAllCalls) {
        isOnCallOnOtherDevice_ = "";
        rejectCallPartyIDs_.clear();
        emit newCallDeviceState();
    }
    emit newInviteState();
    inviteSDP_.clear();
    remoteICECandidates_.clear();
}

void
CallManager::endCall(bool endAllCalls)
{
    stopRingtone();
    session_.end();
    clear(endAllCalls);
}

void
CallManager::refreshTurnServer()
{
    turnURIs_.clear();
    turnServerTimer_.start(2000);
}

void
CallManager::retrieveTurnServer()
{
    turnServerTimer_.stop();
    nhlog::ui()->warn(
      "Skipping legacy TURN server retrieval; VoIP TURN bootstrap is not migrated to "
      "matrix-sdk yet");
}

void
CallManager::playRingtone(const QUrl &ringtone, bool repeat)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
          this,
          [this, ringtone, repeat]() { playRingtone(ringtone, repeat); },
          Qt::QueuedConnection);
        return;
    }

    auto *player = ensurePlayerInitialized();
    nhlog::ui()->debug("Trying to play ringtone {}", ringtone.toString().toStdString());
    player->setLoops(repeat ? QMediaPlayer::Infinite : 1);
    player->setSource(ringtone);
    // player_.audioOutput()->setVolume(100);
    player->play();
}

void
CallManager::stopRingtone()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { stopRingtone(); }, Qt::QueuedConnection);
        return;
    }

    if (player_)
        player_->stop();
}

namespace {
std::vector<std::string>
getTurnURIs(const mtx::responses::TurnServer &turnServer)
{
    // gstreamer expects: turn(s)://username:password@host:port?transport=udp(tcp)
    // where username and password are percent-encoded
    std::vector<std::string> ret;
    for (const auto &uri : turnServer.uris) {
        if (auto c = uri.find(':'); c == std::string::npos) {
            nhlog::ui()->error("Invalid TURN server uri: {}", uri);
            continue;
        } else {
            std::string scheme = std::string(uri, 0, c);
            if (scheme != "turn" && scheme != "turns") {
                nhlog::ui()->error("Invalid TURN server uri: {}", uri);
                continue;
            }

            QString encodedUri =
              QString::fromStdString(scheme) + "://" +
              QUrl::toPercentEncoding(QString::fromStdString(turnServer.username)) + ":" +
              QUrl::toPercentEncoding(QString::fromStdString(turnServer.password)) + "@" +
              QString::fromStdString(std::string(uri, ++c));
            ret.push_back(encodedUri.toStdString());
        }
    }
    return ret;
}
} // namespace

#include "moc_CallManager.cpp"
