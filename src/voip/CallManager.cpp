// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include <QAudioOutput>
#include <QGuiApplication>
#include <QMetaObject>
#include <QRandomGenerator>
#include <QThread>
#include <QUrl>

#include "CallDevices.h"
#include "CallManager.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

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
getTurnURIs(const komai::MatrixTurnServerInfo &turnServer);

komai::voip::CallIceCandidate
toCallIceCandidate(const mtx::events::voip::CallCandidates::Candidate &candidate)
{
    return komai::voip::CallIceCandidate{
      .sdpMid        = candidate.sdpMid,
      .sdpMLineIndex = candidate.sdpMLineIndex,
      .candidate     = candidate.candidate,
    };
}

komai::voip::CallIceCandidateList
toCallIceCandidates(const std::vector<mtx::events::voip::CallCandidates::Candidate> &candidates)
{
    komai::voip::CallIceCandidateList result;
    result.reserve(candidates.size());
    for (const auto &candidate : candidates)
        result.push_back(toCallIceCandidate(candidate));
    return result;
}

struct MatrixCallRoomContext
{
    QString roomId;
    QString displayName;
    QString avatarUrl;
    QString directChatOtherUserId;
    uint64_t memberCount = 0;
};

QString
randomAlphaNumericToken(int length)
{
    static constexpr char Alphabet[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    QString token;
    token.reserve(length);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < length; ++i) {
        token.append(
          QLatin1Char(Alphabet[rng->bounded(static_cast<int>(std::size(Alphabet) - 1))]));
    }

    return token;
}

std::optional<MatrixCallRoomContext>
fetchMatrixCallRoomContext(const QString &roomId)
{
    auto *chatPage        = ChatPage::instance();
    auto *timelineManager = chatPage ? chatPage->timelineManager() : nullptr;
    auto *roomsModel      = timelineManager ? timelineManager->rooms() : nullptr;
    if (!roomsModel)
        return std::nullopt;

    const auto &rooms = roomsModel->matrixJoinedRooms();
    const auto roomIt = rooms.find(roomId);
    if (roomIt == rooms.end())
        return std::nullopt;

    const auto &room = roomIt.value();
    return MatrixCallRoomContext{
      .roomId                = room.roomId,
      .displayName           = room.displayName,
      .avatarUrl             = room.avatarUrl,
      .directChatOtherUserId = room.directChatOtherUserId,
      .memberCount           = room.memberCount,
    };
}

QString
displayNameFromCallRoomContext(const std::optional<MatrixCallRoomContext> &roomContext,
                               const QString &fallbackUserId)
{
    if (!roomContext || roomContext->displayName.trimmed().isEmpty())
        return fallbackUserId;

    return roomContext->displayName;
}

struct MatrixTurnServerFetchResult
{
    uint64_t handleId = 0;
    std::optional<komai::MatrixTurnServerInfo> turnServerInfo;
    QString error;
};
struct MatrixCallEventSendResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString eventType;
    QString error;
    bool ok = false;
};

template<typename WorkFn>
void
queueCallEventSend(CallManager *manager,
                   const QString &roomId,
                   const char *eventTypeName,
                   WorkFn &&workFn)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || roomId.trimmed().isEmpty()) {
        nhlog::ui()->warn("Refusing to send matrix call event '{}' without an active runtime "
                          "handle or room id",
                          eventTypeName);
        return;
    }

    const auto eventType = QString::fromUtf8(eventTypeName);
    komai::qt_worker_task::runQueued(
      manager,
      [handleId, roomId, eventType, workFn = std::forward<WorkFn>(workFn)]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          bool ok = true;
          try {
              workFn(context, handleId, roomId);
          } catch (const std::exception &e) {
              error = QString::fromUtf8(e.what());
              ok    = false;
          }
          return MatrixCallEventSendResult{
            .handleId  = handleId,
            .roomId    = roomId,
            .eventType = eventType,
            .error     = error,
            .ok        = ok,
          };
      },
      [](CallManager *, MatrixCallEventSendResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;
          if (result.ok)
              return;
          nhlog::ui()->warn("Failed to queue matrix call event '{}' for room '{}' on handle {}: {}",
                            result.eventType.toStdString(),
                            result.roomId.toStdString(),
                            result.handleId,
                            result.error.toStdString());
      });
}

std::string
hangUpReasonToString(CallHangUp::Reason reason)
{
    switch (reason) {
    case CallHangUp::Reason::ICEFailed:
        return "ice_failed";
    case CallHangUp::Reason::InviteTimeOut:
        return "invite_timeout";
    case CallHangUp::Reason::ICETimeOut:
        return "ice_timeout";
    case CallHangUp::Reason::UserHangUp:
        return "user_hangup";
    case CallHangUp::Reason::UserMediaFailed:
        return "user_media_failed";
    case CallHangUp::Reason::UserBusy:
        return "user_busy";
    case CallHangUp::Reason::UnknownError:
        return "unknown_error";
    case CallHangUp::Reason::User:
        return "user";
    }
    return "user_hangup";
}

CallHangUp::Reason
hangUpReasonFromString(const std::string &reason)
{
    if (reason == "ice_failed")
        return CallHangUp::Reason::ICEFailed;
    if (reason == "invite_timeout")
        return CallHangUp::Reason::InviteTimeOut;
    if (reason == "ice_timeout")
        return CallHangUp::Reason::ICETimeOut;
    if (reason == "user_media_failed")
        return CallHangUp::Reason::UserMediaFailed;
    if (reason == "user_busy")
        return CallHangUp::Reason::UserBusy;
    if (reason == "unknown_error")
        return CallHangUp::Reason::UnknownError;
    if (reason == "user")
        return CallHangUp::Reason::User;
    return CallHangUp::Reason::UserHangUp;
}

std::string
sdoTypeToString(RTCSessionDescriptionInit::Type type)
{
    return type == RTCSessionDescriptionInit::Type::Answer ? "answer" : "offer";
}

} // namespace

void
CallManager::sendCallInvite(const QString &roomId, const CallInvite &content)
{
    const auto callId   = content.call_id;
    const auto partyId  = content.party_id;
    const auto version  = content.version;
    const auto lifetime = content.lifetime;
    const auto invitee  = content.invitee;
    const auto sdp      = content.offer.sdp;
    const auto sdpType  = sdoTypeToString(content.offer.type);

    queueCallEventSend(
      this,
      roomId,
      "m.call.invite",
      [callId, partyId, version, lifetime, invitee, sdp, sdpType](
        komai::matrix_backend::BlockingCallContext context,
        uint64_t handleId,
        const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallInvite(
            context, handleId, roomId, callId, partyId, version, lifetime, invitee, sdp, sdpType);
      });
}

void
CallManager::sendCallCandidates(const QString &roomId,
                                const std::string &callId,
                                const std::string &partyId,
                                const komai::voip::CallIceCandidateList &candidates,
                                const std::string &version)
{
    queueCallEventSend(
      this,
      roomId,
      "m.call.candidates",
      [callId, partyId, version, candidates](komai::matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallCandidates(
            context, handleId, roomId, callId, partyId, version, candidates);
      });
}

void
CallManager::sendCallAnswer(const QString &roomId, const CallAnswer &content)
{
    const auto callId  = content.call_id;
    const auto partyId = content.party_id;
    const auto version = content.version;
    const auto sdp     = content.answer.sdp;
    const auto sdpType = sdoTypeToString(content.answer.type);

    queueCallEventSend(
      this,
      roomId,
      "m.call.answer",
      [callId, partyId, version, sdp, sdpType](komai::matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallAnswer(
            context, handleId, roomId, callId, partyId, version, sdp, sdpType);
      });
}

void
CallManager::sendCallHangUp(const QString &roomId, const CallHangUp &content)
{
    const auto callId  = content.call_id;
    const auto partyId = content.party_id;
    const auto version = content.version;
    const auto reason  = hangUpReasonToString(content.reason);

    queueCallEventSend(
      this,
      roomId,
      "m.call.hangup",
      [callId, partyId, version, reason](komai::matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallHangUp(
            context, handleId, roomId, callId, partyId, version, reason);
      });
}

void
CallManager::sendCallSelectAnswer(const QString &roomId, const CallSelectAnswer &content)
{
    const auto callId          = content.call_id;
    const auto partyId         = content.party_id;
    const auto version         = content.version;
    const auto selectedPartyId = content.selected_party_id;

    queueCallEventSend(this,
                       roomId,
                       "m.call.select_answer",
                       [callId, partyId, version, selectedPartyId](
                         komai::matrix_backend::BlockingCallContext context,
                         uint64_t handleId,
                         const QString &roomId) {
                           komai::MatrixBackendRuntimeService::sendCallSelectAnswer(
                             context, handleId, roomId, callId, partyId, version, selectedPartyId);
                       });
}

void
CallManager::sendCallReject(const QString &roomId, const CallReject &content)
{
    const auto callId  = content.call_id;
    const auto partyId = content.party_id;
    const auto version = content.version;

    queueCallEventSend(
      this,
      roomId,
      "m.call.reject",
      [callId, partyId, version](komai::matrix_backend::BlockingCallContext context,
                                 uint64_t handleId,
                                 const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallReject(
            context, handleId, roomId, callId, partyId, version);
      });
}

void
CallManager::sendCallNegotiate(const QString &roomId, const CallNegotiate &content)
{
    const auto callId   = content.call_id;
    const auto partyId  = content.party_id;
    const auto lifetime = content.lifetime;
    const auto sdp      = content.description.sdp;
    const auto sdpType  = sdoTypeToString(content.description.type);

    queueCallEventSend(
      this,
      roomId,
      "m.call.negotiate",
      [callId, partyId, lifetime, sdp, sdpType](komai::matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallNegotiate(
            context, handleId, roomId, callId, partyId, lifetime, sdp, sdpType);
      });
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
  , partyid_(randomAlphaNumericToken(8).toStdString())
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

    connect(&session_,
            &WebRTCSession::offerCreated,
            this,
            [this](const std::string &sdp, const komai::voip::CallIceCandidateList &candidates) {
                nhlog::ui()->debug("WebRTC: call id: {} - sending offer", callid_);
                sendCallInvite(roomid_,
                               CallInvite{callid_,
                                          partyid_,
                                          SDO{sdp, SDO::Type::Offer},
                                          callPartyVersion_,
                                          timeoutms_,
                                          invitee_});
                sendCallCandidates(roomid_, callid_, partyid_, candidates, callPartyVersion_);
                std::string callid(callid_);
                QTimer::singleShot(timeoutms_, this, [this, callid]() {
                    if (session_.state() == webrtc::State::OFFERSENT && callid == callid_) {
                        hangUp(CallHangUp::Reason::InviteTimeOut);
                        emit ChatPage::instance()->showNotification(
                          QStringLiteral("The remote side failed to pick up."));
                    }
                });
            });

    connect(&session_,
            &WebRTCSession::answerCreated,
            this,
            [this](const std::string &sdp, const komai::voip::CallIceCandidateList &candidates) {
                nhlog::ui()->debug("WebRTC: call id: {} - sending answer", callid_);
                sendCallAnswer(
                  roomid_,
                  CallAnswer{callid_, partyid_, callPartyVersion_, SDO{sdp, SDO::Type::Answer}});
                sendCallCandidates(roomid_, callid_, partyid_, candidates, callPartyVersion_);
            });

    connect(&session_,
            &WebRTCSession::newICECandidate,
            this,
            [this](const komai::voip::CallIceCandidate &candidate) {
                nhlog::ui()->debug("WebRTC: call id: {} - sending ice candidate", callid_);
                sendCallCandidates(roomid_, callid_, partyid_, {candidate}, callPartyVersion_);
            });

    connect(&turnServerTimer_, &QTimer::timeout, this, &CallManager::retrieveTurnServer);

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

    callType_ = callType;
    roomid_   = roomid;
    generateCallID();

    const auto roomContext = fetchMatrixCallRoomContext(roomid);
    if (!roomContext || roomContext->directChatOtherUserId.trimmed().isEmpty()) {
        emit ChatPage::instance()->showNotification(
          QStringLiteral("Calls are currently supported only in direct rooms."));
        return;
    }

    const auto calleeId = roomContext->directChatOtherUserId;

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
        if (callParty_ == calleeId) {
            if (callType == callType_)
                acceptInvite();
            else {
                emit ChatPage::instance()->showNotification(
                  QStringLiteral("Can't place call. Call types do not match"));
                sendCallHangUp(
                  roomid_,
                  CallHangUp{callid_, partyid_, callPartyVersion_, CallHangUp::Reason::UserBusy});
            }
        } else {
            emit ChatPage::instance()->showNotification(
              QStringLiteral("Already on a call with a different user"));
            sendCallHangUp(
              roomid_,
              CallHangUp{callid_, partyid_, callPartyVersion_, CallHangUp::Reason::UserBusy});
        }
        return;
    }

    session_.setTurnServers(turnURIs_);
    std::string strCallType =
      callType_ == CallType::VOICE ? "voice" : (callType_ == CallType::VIDEO ? "video" : "screen");

    nhlog::ui()->debug("WebRTC: call id: {} - creating {} invite", callid_, strCallType);
    callParty_            = calleeId;
    callPartyDisplayName_ = displayNameFromCallRoomContext(roomContext, calleeId);
    callPartyAvatarUrl_   = roomContext->avatarUrl;
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
        sendCallHangUp(roomid_, CallHangUp{callid_, partyid_, callPartyVersion_, reason});
        endCall();
    }
}

namespace {
template<typename EventContent>
mtx::events::RoomEvent<EventContent>
makeCallRoomEvent(const QString &roomId,
                  const QString &senderId,
                  const QString &eventId,
                  EventContent content)
{
    mtx::events::RoomEvent<EventContent> event;
    event.sender           = senderId.toStdString();
    event.content          = std::move(content);
    event.event_id         = eventId.toStdString();
    event.room_id          = roomId.toStdString();
    event.origin_server_ts = 0;
    return event;
}

RTCSessionDescriptionInit::Type
sdoTypeFromString(const std::string &type)
{
    return type == "answer" ? RTCSessionDescriptionInit::Type::Answer
                            : RTCSessionDescriptionInit::Type::Offer;
}
} // namespace

void
CallManager::handleCallInvite(const QString &roomId,
                              const QString &senderId,
                              const QString &eventId,
                              const std::string &callId,
                              const std::string &partyId,
                              const std::string &version,
                              uint32_t lifetime,
                              const std::string &invitee,
                              const std::string &offerSdp,
                              const std::string &offerType)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    CallInvite content;
    content.call_id  = callId;
    content.party_id = partyId;
    content.version  = version;
    content.lifetime = lifetime;
    content.invitee  = invitee;
    content.offer    = SDO{offerSdp, sdoTypeFromString(offerType)};
    handleEvent(makeCallRoomEvent(roomId, senderId, eventId, std::move(content)));
#else
    Q_UNUSED(roomId)
    Q_UNUSED(senderId)
    Q_UNUSED(eventId)
    Q_UNUSED(callId)
    Q_UNUSED(partyId)
    Q_UNUSED(version)
    Q_UNUSED(lifetime)
    Q_UNUSED(invitee)
    Q_UNUSED(offerSdp)
    Q_UNUSED(offerType)
#endif
}

void
CallManager::handleCallCandidates(const QString &roomId,
                                  const QString &senderId,
                                  const QString &eventId,
                                  const std::string &callId,
                                  const std::string &partyId,
                                  const std::string &version,
                                  const komai::voip::CallIceCandidateList &candidates)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    CallCandidates content;
    content.call_id  = callId;
    content.party_id = partyId;
    content.version  = version;
    content.candidates.reserve(candidates.size());
    for (const auto &c : candidates) {
        content.candidates.push_back(CallCandidates::Candidate{
          .sdpMid        = c.sdpMid,
          .sdpMLineIndex = c.sdpMLineIndex,
          .candidate     = c.candidate,
        });
    }
    handleEvent(makeCallRoomEvent(roomId, senderId, eventId, std::move(content)));
#else
    Q_UNUSED(roomId)
    Q_UNUSED(senderId)
    Q_UNUSED(eventId)
    Q_UNUSED(callId)
    Q_UNUSED(partyId)
    Q_UNUSED(version)
    Q_UNUSED(candidates)
#endif
}

void
CallManager::handleCallAnswer(const QString &roomId,
                              const QString &senderId,
                              const QString &eventId,
                              const std::string &callId,
                              const std::string &partyId,
                              const std::string &version,
                              const std::string &answerSdp,
                              const std::string &answerType)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    CallAnswer content;
    content.call_id  = callId;
    content.party_id = partyId;
    content.version  = version;
    content.answer   = SDO{answerSdp, sdoTypeFromString(answerType)};
    handleEvent(makeCallRoomEvent(roomId, senderId, eventId, std::move(content)));
#else
    Q_UNUSED(roomId)
    Q_UNUSED(senderId)
    Q_UNUSED(eventId)
    Q_UNUSED(callId)
    Q_UNUSED(partyId)
    Q_UNUSED(version)
    Q_UNUSED(answerSdp)
    Q_UNUSED(answerType)
#endif
}

void
CallManager::handleCallHangUp(const QString &roomId,
                              const QString &senderId,
                              const QString &eventId,
                              const std::string &callId,
                              const std::string &partyId,
                              const std::string &version,
                              const std::string &reason)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    CallHangUp content;
    content.call_id  = callId;
    content.party_id = partyId;
    content.version  = version;
    content.reason   = hangUpReasonFromString(reason);
    handleEvent(makeCallRoomEvent(roomId, senderId, eventId, std::move(content)));
#else
    Q_UNUSED(roomId)
    Q_UNUSED(senderId)
    Q_UNUSED(eventId)
    Q_UNUSED(callId)
    Q_UNUSED(partyId)
    Q_UNUSED(version)
    Q_UNUSED(reason)
#endif
}

void
CallManager::handleCallSelectAnswer(const QString &roomId,
                                    const QString &senderId,
                                    const QString &eventId,
                                    const std::string &callId,
                                    const std::string &partyId,
                                    const std::string &version,
                                    const std::string &selectedPartyId)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    CallSelectAnswer content;
    content.call_id           = callId;
    content.party_id          = partyId;
    content.version           = version;
    content.selected_party_id = selectedPartyId;
    handleEvent(makeCallRoomEvent(roomId, senderId, eventId, std::move(content)));
#else
    Q_UNUSED(roomId)
    Q_UNUSED(senderId)
    Q_UNUSED(eventId)
    Q_UNUSED(callId)
    Q_UNUSED(partyId)
    Q_UNUSED(version)
    Q_UNUSED(selectedPartyId)
#endif
}

void
CallManager::handleCallReject(const QString &roomId,
                              const QString &senderId,
                              const QString &eventId,
                              const std::string &callId,
                              const std::string &partyId,
                              const std::string &version)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    CallReject content;
    content.call_id  = callId;
    content.party_id = partyId;
    content.version  = version;
    handleEvent(makeCallRoomEvent(roomId, senderId, eventId, std::move(content)));
#else
    Q_UNUSED(roomId)
    Q_UNUSED(senderId)
    Q_UNUSED(eventId)
    Q_UNUSED(callId)
    Q_UNUSED(partyId)
    Q_UNUSED(version)
#endif
}

void
CallManager::handleCallNegotiate(const QString &roomId,
                                 const QString &senderId,
                                 const QString &eventId,
                                 const std::string &callId,
                                 const std::string &partyId,
                                 uint32_t lifetime,
                                 const std::string &descSdp,
                                 const std::string &descType)
{
    if (!UserSettings::instance()->callsLegacyEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    CallNegotiate content;
    content.call_id     = callId;
    content.party_id    = partyId;
    content.lifetime    = lifetime;
    content.description = SDO{descSdp, sdoTypeFromString(descType)};
    handleEvent(makeCallRoomEvent(roomId, senderId, eventId, std::move(content)));
#else
    Q_UNUSED(roomId)
    Q_UNUSED(senderId)
    Q_UNUSED(eventId)
    Q_UNUSED(callId)
    Q_UNUSED(partyId)
    Q_UNUSED(lifetime)
    Q_UNUSED(descSdp)
    Q_UNUSED(descType)
#endif
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

    const auto roomId      = QString::fromStdString(callInviteEvent.room_id);
    const auto roomContext = fetchMatrixCallRoomContext(roomId);
    callPartyVersion_      = callInviteEvent.content.version;

    const QString &ringtone = UserSettings::instance()->callsAudioRingtone();
    bool sharesRoom         = true;

    const auto callerUserId      = QString::fromStdString(callInviteEvent.sender);
    const auto callerDisplayName = displayNameFromCallRoomContext(roomContext, callerUserId);
    const auto callerAvatarUrl   = roomContext ? roomContext->avatarUrl : QString{};
    if (isOnCall() || isOnCallOnOtherDevice()) {
        if (isOnCallOnOtherDevice_ != "")
            return;
        if (callParty_.toStdString() == callInviteEvent.sender) {
            if (session_.state() == webrtc::State::OFFERSENT) {
                if (callid_ > callInviteEvent.content.call_id) {
                    endCall();
                    callParty_            = callerUserId;
                    callPartyDisplayName_ = callerDisplayName;
                    callPartyAvatarUrl_   = callerAvatarUrl;

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

    const auto memberCount   = roomContext ? roomContext->memberCount : 0;
    const bool knowRoomShape = roomContext && memberCount > 0;
    if (callPartyVersion_ == "0") {
        if (knowRoomShape && memberCount != 2) {
            sendCallHangUp(QString::fromStdString(callInviteEvent.room_id),
                           CallHangUp{callInviteEvent.content.call_id,
                                      partyid_,
                                      callPartyVersion_,
                                      CallHangUp::Reason::InviteTimeOut});
            return;
        }
    } else {
        if (callerUserId == utils::localUser() &&
            callInviteEvent.content.party_id == partyid_) // remote echo
            return;

        if (knowRoomShape) {
            if (memberCount == 2 || // general call in room with two members
                (memberCount == 1 &&
                 partyid_ !=
                   callInviteEvent.content.party_id) ||  // self call, ring if not the same party_id
                callInviteEvent.content.invitee == "" || // empty, meant for everyone
                callInviteEvent.content.invitee ==
                  utils::localUser().toStdString()) // meant specifically for local user
            {
                if (memberCount > 2) {
                    // check if shares room
                    sharesRoom = checkSharesRoom(QString::fromStdString(callInviteEvent.room_id),
                                                 callInviteEvent.content.invitee);
                }
            } else {
                sendCallHangUp(QString::fromStdString(callInviteEvent.room_id),
                               CallHangUp{callInviteEvent.content.call_id,
                                          partyid_,
                                          callPartyVersion_,
                                          CallHangUp::Reason::InviteTimeOut});
                return;
            }
        }
    }

    // ring if not mute or does not have direct message room
    if (ringtone != QLatin1String("Mute") && sharesRoom)
        playRingtone(ringtone == QLatin1String("Default")
                       ? QUrl(QStringLiteral("qrc:/media/media/ring.ogg"))
                       : QUrl::fromLocalFile(ringtone),
                     true);

    callParty_            = callerUserId;
    callPartyDisplayName_ = callerDisplayName;
    callPartyAvatarUrl_   = callerAvatarUrl;

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
        sendCallReject(roomid_, CallReject{callid_, partyid_, callPartyVersion_});
    }
    if (!callid_.empty()) {
        nhlog::ui()->debug("WebRTC: call id: {} - rejecting call", callid_);
        sendCallReject(roomid_, CallReject{callid_, partyid_, callPartyVersion_});
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
            session_.acceptICECandidates(
              toCallIceCandidates(callCandidatesEvent.content.candidates));
        else {
            // CallInvite has been received and we're awaiting localUser to accept or
            // reject the call
            for (const auto &c : callCandidatesEvent.content.candidates)
                remoteICECandidates_.push_back(toCallIceCandidate(c));
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
    sendCallSelectAnswer(
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
            sendCallSelectAnswer(
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
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        nhlog::ui()->warn("Skipping TURN server retrieval because no matrix-sdk runtime handle "
                          "is active");
        return;
    }

    komai::qt_worker_task::runQueued(
      this,
      [handleId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          auto turnServerInfo =
            komai::MatrixBackendRuntimeService::fetchTurnServerInfo(context, handleId, &error);

          return MatrixTurnServerFetchResult{
            .handleId       = handleId,
            .turnServerInfo = std::move(turnServerInfo),
            .error          = std::move(error),
          };
      },
      [](CallManager *manager, MatrixTurnServerFetchResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.turnServerInfo) {
              nhlog::ui()->warn("Failed to fetch TURN server info on handle {}: {}",
                                result.handleId,
                                result.error.toStdString());
              return;
          }

          manager->applyTurnServerInfo(*result.turnServerInfo);
      });
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
getTurnURIs(const komai::MatrixTurnServerInfo &turnServer)
{
    // gstreamer expects: turn(s)://username:password@host:port?transport=udp(tcp)
    // where username and password are percent-encoded
    std::vector<std::string> ret;
    for (const auto &uri : turnServer.uris) {
        const auto uriString = uri.toStdString();
        if (auto c = uriString.find(':'); c == std::string::npos) {
            nhlog::ui()->error("Invalid TURN server uri: {}", uriString);
            continue;
        } else {
            std::string scheme = std::string(uriString, 0, c);
            if (scheme != "turn" && scheme != "turns") {
                nhlog::ui()->error("Invalid TURN server uri: {}", uriString);
                continue;
            }

            QString encodedUri = QString::fromStdString(scheme) + "://";
            if (!turnServer.username.isEmpty() || !turnServer.password.isEmpty()) {
                encodedUri += QUrl::toPercentEncoding(turnServer.username) + ":" +
                              QUrl::toPercentEncoding(turnServer.password) + "@";
            }

            encodedUri += QString::fromStdString(std::string(uriString, ++c));
            ret.push_back(encodedUri.toStdString());
        }
    }
    return ret;
}
} // namespace

void
CallManager::applyTurnServerInfo(const komai::MatrixTurnServerInfo &info)
{
    if (info.uris.isEmpty()) {
        nhlog::net()->info("Homeserver returned no TURN server URIs");
    } else {
        nhlog::net()->info("TURN server(s) retrieved from homeserver:");
        nhlog::net()->info("username: {}", info.username.toStdString());
        nhlog::net()->info("ttl: {} seconds", info.ttlSeconds);
        for (const auto &uri : info.uris)
            nhlog::net()->info("uri: {}", uri.toStdString());
    }

    // Request new credentials close to expiry.
    turnURIs_ = getTurnURIs(info);

    uint64_t ttl = std::max(info.ttlSeconds, uint64_t{3600});
    if (!info.uris.isEmpty() && info.ttlSeconds < 3600)
        nhlog::net()->warn("Setting ttl to 1 hour");
    turnServerTimer_.setInterval(std::chrono::seconds(ttl) * 10 / 9);
}

#include "moc_CallManager.cpp"
