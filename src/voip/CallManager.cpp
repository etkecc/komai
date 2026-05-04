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

using webrtc::CallType;
using webrtc::ScreenShareType;

namespace {
std::vector<std::string>
getTurnURIs(const komai::MatrixTurnServerInfo &turnServer);

bool
preMatrixRtcCallsSettingEnabled()
{
    return UserSettings::instance()->callsLegacyEnabled();
}

uint64_t
activeMatrixRuntimeHandleId()
{
    auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

bool
isCurrentMatrixRuntimeHandle(uint64_t handleId)
{
    auto *mainWindow = MainWindow::instance();
    return mainWindow && mainWindow->matrixBackendHandleId() == handleId;
}

struct MatrixCallRoomContext
{
    QString roomId;
    QString displayName;
    QString avatarUrl;
    QString directChatOtherUserId;
    uint64_t memberCount = 0;
    bool isDirect        = false;
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
      .isDirect              = room.isDirect,
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
    const auto handleId = activeMatrixRuntimeHandleId();
    if (handleId == 0 || roomId.trimmed().isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to send matrix call event '{}' without an active runtime "
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
          if (!isCurrentMatrixRuntimeHandle(result.handleId))
              return;
          if (result.ok)
              return;
          komai::logging::ui()->warn(
            "Failed to queue matrix call event '{}' for room '{}' on handle {}: {}",
            result.eventType.toStdString(),
            result.roomId.toStdString(),
            result.handleId,
            result.error.toStdString());
      });
}

std::string
hangUpReasonToString(komai::voip::CallHangUpReason reason)
{
    switch (reason) {
    case komai::voip::CallHangUpReason::ICEFailed:
        return "ice_failed";
    case komai::voip::CallHangUpReason::InviteTimeOut:
        return "invite_timeout";
    case komai::voip::CallHangUpReason::ICETimeOut:
        return "ice_timeout";
    case komai::voip::CallHangUpReason::UserHangUp:
        return "user_hangup";
    case komai::voip::CallHangUpReason::UserMediaFailed:
        return "user_media_failed";
    case komai::voip::CallHangUpReason::UserBusy:
        return "user_busy";
    case komai::voip::CallHangUpReason::UnknownError:
        return "unknown_error";
    case komai::voip::CallHangUpReason::User:
        return "user";
    }
    return "user_hangup";
}

komai::voip::CallHangUpReason
hangUpReasonFromString(const std::string &reason)
{
    if (reason == "ice_failed")
        return komai::voip::CallHangUpReason::ICEFailed;
    if (reason == "invite_timeout")
        return komai::voip::CallHangUpReason::InviteTimeOut;
    if (reason == "ice_timeout")
        return komai::voip::CallHangUpReason::ICETimeOut;
    if (reason == "user_media_failed")
        return komai::voip::CallHangUpReason::UserMediaFailed;
    if (reason == "user_busy")
        return komai::voip::CallHangUpReason::UserBusy;
    if (reason == "unknown_error")
        return komai::voip::CallHangUpReason::UnknownError;
    if (reason == "user")
        return komai::voip::CallHangUpReason::User;
    return komai::voip::CallHangUpReason::UserHangUp;
}

std::string
callSdpTypeToString(komai::voip::CallSdpType type)
{
    return type == komai::voip::CallSdpType::Answer ? "answer" : "offer";
}

} // namespace

void
CallManager::sendCallInvite(const QString &roomId,
                            const std::string &callId,
                            const std::string &partyId,
                            const std::string &version,
                            uint32_t lifetime,
                            const std::string &invitee,
                            const std::string &offerSdp,
                            komai::voip::CallSdpType offerType)
{
    const auto sdpType = callSdpTypeToString(offerType);

    queueCallEventSend(this,
                       roomId,
                       "m.call.invite",
                       [callId, partyId, version, lifetime, invitee, offerSdp, sdpType](
                         komai::matrix_backend::BlockingCallContext context,
                         uint64_t handleId,
                         const QString &roomId) {
                           komai::MatrixBackendRuntimeService::sendCallInvite(context,
                                                                              handleId,
                                                                              roomId,
                                                                              callId,
                                                                              partyId,
                                                                              version,
                                                                              lifetime,
                                                                              invitee,
                                                                              offerSdp,
                                                                              sdpType);
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
CallManager::sendCallAnswer(const QString &roomId,
                            const std::string &callId,
                            const std::string &partyId,
                            const std::string &version,
                            const std::string &answerSdp,
                            komai::voip::CallSdpType answerType)
{
    const auto sdpType = callSdpTypeToString(answerType);

    queueCallEventSend(
      this,
      roomId,
      "m.call.answer",
      [callId, partyId, version, answerSdp, sdpType](
        komai::matrix_backend::BlockingCallContext context,
        uint64_t handleId,
        const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallAnswer(
            context, handleId, roomId, callId, partyId, version, answerSdp, sdpType);
      });
}

void
CallManager::sendCallHangUp(const QString &roomId,
                            const std::string &callId,
                            const std::string &partyId,
                            const std::string &version,
                            komai::voip::CallHangUpReason reason)
{
    const auto reasonValue = hangUpReasonToString(reason);

    queueCallEventSend(
      this,
      roomId,
      "m.call.hangup",
      [callId, partyId, version, reasonValue](komai::matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallHangUp(
            context, handleId, roomId, callId, partyId, version, reasonValue);
      });
}

void
CallManager::sendCallSelectAnswer(const QString &roomId,
                                  const std::string &callId,
                                  const std::string &partyId,
                                  const std::string &version,
                                  const std::string &selectedPartyId)
{
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
CallManager::sendCallReject(const QString &roomId,
                            const std::string &callId,
                            const std::string &partyId,
                            const std::string &version)
{
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
CallManager::sendCallNegotiate(const QString &roomId,
                               const std::string &callId,
                               const std::string &partyId,
                               uint32_t lifetime,
                               const std::string &descSdp,
                               komai::voip::CallSdpType descType)
{
    const auto sdpType = callSdpTypeToString(descType);

    queueCallEventSend(
      this,
      roomId,
      "m.call.negotiate",
      [callId, partyId, lifetime, descSdp, sdpType](
        komai::matrix_backend::BlockingCallContext context,
        uint64_t handleId,
        const QString &roomId) {
          komai::MatrixBackendRuntimeService::sendCallNegotiate(
            context, handleId, roomId, callId, partyId, lifetime, descSdp, sdpType);
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
                komai::logging::ui()->debug("WebRTC: call id: {} - sending offer", callid_);
                sendCallInvite(roomid_,
                               callid_,
                               partyid_,
                               callPartyVersion_,
                               timeoutms_,
                               invitee_,
                               sdp,
                               komai::voip::CallSdpType::Offer);
                sendCallCandidates(roomid_, callid_, partyid_, candidates, callPartyVersion_);
                std::string callid(callid_);
                QTimer::singleShot(timeoutms_, this, [this, callid]() {
                    if (session_.state() == webrtc::State::OFFERSENT && callid == callid_) {
                        hangUp(komai::voip::CallHangUpReason::InviteTimeOut);
                        emit ChatPage::instance()->showNotification(
                          QStringLiteral("The remote side failed to pick up."));
                    }
                });
            });

    connect(
      &session_,
      &WebRTCSession::answerCreated,
      this,
      [this](const std::string &sdp, const komai::voip::CallIceCandidateList &candidates) {
          komai::logging::ui()->debug("WebRTC: call id: {} - sending answer", callid_);
          sendCallAnswer(
            roomid_, callid_, partyid_, callPartyVersion_, sdp, komai::voip::CallSdpType::Answer);
          sendCallCandidates(roomid_, callid_, partyid_, candidates, callPartyVersion_);
      });

    connect(&session_,
            &WebRTCSession::newICECandidate,
            this,
            [this](const komai::voip::CallIceCandidate &candidate) {
                komai::logging::ui()->debug("WebRTC: call id: {} - sending ice candidate", callid_);
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
            hangUp(komai::voip::CallHangUpReason::ICEFailed);
            break;
        }
        default:
            break;
        }
        emit newCallState();
    });

    connect(
      &CallDevices::instance(), &CallDevices::devicesChanged, this, &CallManager::devicesChanged);
    connect(UserSettings::instance().data(),
            &UserSettings::callsLegacyEnabledChanged,
            this,
            &CallManager::preMatrixRtcCallsEnabledChanged);

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
                    komai::logging::ui()->debug(
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
                        komai::logging::ui()->error("WebRTC: valid ringtone file not found");
                        break;
                    case QMediaPlayer::AccessDeniedError:
                        komai::logging::ui()->error("WebRTC: access to ringtone file denied");
                        break;
                    default:
                        komai::logging::ui()->error("WebRTC: unable to play ringtone, {}",
                                                    errorString.toStdString());
                        break;
                    }
                });
    });

    return player_.get();
}

bool
CallManager::screenShareUsesWindowPicker(ScreenShareType type)
{
    return type == ScreenShareType::X11 || type == ScreenShareType::D3D11;
}

void
CallManager::sendInvite(const QString &roomid, CallType callType, unsigned int windowIndex)
{
    if (!preMatrixRtcCallsSettingEnabled())
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
    // Legacy 1:1 calls are supported in any room our classification flags as
    // direct, plus any room with exactly two active members (covers 2-member
    // rooms that lack m.direct / heroes and so come back as is_direct=false
    // from the matrix-sdk classification).
    if (!roomContext || !(roomContext->isDirect || roomContext->memberCount == 2)) {
        emit ChatPage::instance()->showNotification(
          QStringLiteral("Calls are currently supported only in direct chats."));
        return;
    }

    // May be empty when neither m.direct nor heroes resolved a partner; the
    // rest of the code path tolerates that (invitee is optional in v0).
    const auto calleeId = roomContext->directChatOtherUserId;

#ifdef GSTREAMER_AVAILABLE
    if (callType == CallType::SCREEN) {
        if (screenShareUsesWindowPicker(screenShareType_)) {
            if (windows_.empty() || windowIndex >= windows_.size()) {
                komai::logging::ui()->error("WebRTC: window index out of range");
                return;
            }
        } else {
            ScreenCastPortal &sc_portal = ScreenCastPortal::instance();
            if (sc_portal.getStream() == nullptr) {
                komai::logging::ui()->error("xdg-desktop-portal stream not started");
                return;
            }
        }
    }
#endif

    if (haveCallInvite_) {
        komai::logging::ui()->debug("WebRTC: Discarding outbound call for inbound call. "
                                    "localUser is polite party");
        if (callParty_ == calleeId) {
            if (callType == callType_)
                acceptInvite();
            else {
                emit ChatPage::instance()->showNotification(
                  QStringLiteral("Can't place call. Call types do not match"));
                sendCallHangUp(roomid_,
                               callid_,
                               partyid_,
                               callPartyVersion_,
                               komai::voip::CallHangUpReason::UserBusy);
            }
        } else {
            emit ChatPage::instance()->showNotification(
              QStringLiteral("Already on a call with a different user"));
            sendCallHangUp(roomid_,
                           callid_,
                           partyid_,
                           callPartyVersion_,
                           komai::voip::CallHangUpReason::UserBusy);
        }
        return;
    }

    session_.setTurnServers(turnURIs_);
    std::string strCallType =
      callType_ == CallType::VOICE ? "voice" : (callType_ == CallType::VIDEO ? "video" : "screen");

    komai::logging::ui()->debug("WebRTC: call id: {} - creating {} invite", callid_, strCallType);
    callParty_            = calleeId;
    callPartyDisplayName_ = displayNameFromCallRoomContext(roomContext, calleeId);
    callPartyAvatarUrl_   = roomContext->avatarUrl;
    invitee_              = callParty_.toStdString();
    emit newInviteState();
    playRingtone(QUrl(QStringLiteral("qrc:/media/media/ringback.ogg")), true);

    uint32_t shareWindowId =
      callType == CallType::SCREEN && screenShareUsesWindowPicker(screenShareType_)
        ? windows_[windowIndex].second
        : 0;
    if (!session_.createOffer(callType, screenShareType_, shareWindowId)) {
        emit ChatPage::instance()->showNotification(QStringLiteral("Problem setting up call."));
        endCall();
    }
}

namespace {
std::string
callHangUpReasonString(komai::voip::CallHangUpReason reason)
{
    switch (reason) {
    case komai::voip::CallHangUpReason::ICEFailed:
        return "ICE failed";
    case komai::voip::CallHangUpReason::InviteTimeOut:
        return "Invite time out";
    case komai::voip::CallHangUpReason::ICETimeOut:
        return "ICE time out";
    case komai::voip::CallHangUpReason::UserHangUp:
        return "User hung up";
    case komai::voip::CallHangUpReason::UserMediaFailed:
        return "User media failed";
    case komai::voip::CallHangUpReason::UserBusy:
        return "User busy";
    case komai::voip::CallHangUpReason::UnknownError:
        return "Unknown error";
    default:
        return "User";
    }
}
} // namespace

void
CallManager::hangUp(komai::voip::CallHangUpReason reason)
{
    if (!callid_.empty()) {
        komai::logging::ui()->debug(
          "WebRTC: call id: {} - hanging up ({})", callid_, callHangUpReasonString(reason));
        sendCallHangUp(roomid_, callid_, partyid_, callPartyVersion_, reason);
        endCall();
    }
}

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
    if (!preMatrixRtcCallsSettingEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    Q_UNUSED(eventId)
    Q_UNUSED(lifetime)
    Q_UNUSED(offerType)

    const auto localUserId    = utils::localUser();
    const auto localUserIdStd = localUserId.toStdString();

    const char video[] = "m=video";
    bool isVideo       = std::search(offerSdp.cbegin(),
                               offerSdp.cend(),
                               std::cbegin(video),
                               std::cend(video) - 1,
                               [](unsigned char c1, unsigned char c2) {
                                   return std::tolower(c1) == std::tolower(c2);
                               }) != offerSdp.cend();
    komai::logging::ui()->debug("WebRTC: call id: {} - incoming {} CallInvite from ({},{}) ",
                                callId,
                                (isVideo ? "video" : "voice"),
                                senderId.toStdString(),
                                partyId);

    if (callId.empty())
        return;

    if (senderId.toStdString() == localUserIdStd) {
        if (partyId == partyid_) {
            return;
        } else if (invitee != localUserIdStd) {
            isOnCallOnOtherDevice_ = callId;
            emit newCallDeviceState();
            komai::logging::ui()->debug("WebRTC: User is on call on other device.");
            return;
        }
    }

    const auto roomContext = fetchMatrixCallRoomContext(roomId);
    callPartyVersion_      = version;

    const QString &ringtone = UserSettings::instance()->callsAudioRingtone();
    bool sharesRoom         = true;

    const auto callerUserId      = senderId;
    const auto callerDisplayName = displayNameFromCallRoomContext(roomContext, callerUserId);
    const auto callerAvatarUrl   = roomContext ? roomContext->avatarUrl : QString{};
    if (isOnCall() || isOnCallOnOtherDevice()) {
        if (isOnCallOnOtherDevice_ != "")
            return;
        if (callParty_ == callerUserId) {
            if (session_.state() == webrtc::State::OFFERSENT) {
                if (callid_ > callId) {
                    endCall();
                    callParty_            = callerUserId;
                    callPartyDisplayName_ = callerDisplayName;
                    callPartyAvatarUrl_   = callerAvatarUrl;

                    roomid_ = roomId;
                    callid_ = callId;
                    remoteICECandidates_.clear();
                    haveCallInvite_ = true;
                    callType_       = isVideo ? CallType::VIDEO : CallType::VOICE;
                    inviteSDP_      = offerSdp;
                    emit newInviteState();
                    acceptInvite();
                }
                return;
            } else if (session_.state() < webrtc::State::OFFERSENT) {
                endCall();
            } else {
                return;
            }
        } else {
            return;
        }
    }

    const auto memberCount   = roomContext ? roomContext->memberCount : 0;
    const bool knowRoomShape = roomContext && memberCount > 0;
    if (callPartyVersion_ == "0") {
        if (knowRoomShape && memberCount != 2) {
            sendCallHangUp(roomId,
                           callId,
                           partyid_,
                           callPartyVersion_,
                           komai::voip::CallHangUpReason::InviteTimeOut);
            return;
        }
    } else {
        if (callerUserId == localUserId && partyId == partyid_)
            return;

        if (knowRoomShape) {
            if (memberCount == 2 || (memberCount == 1 && partyid_ != partyId) || invitee.empty() ||
                invitee == localUserIdStd) {
                if (memberCount > 2) {
                    sharesRoom = checkSharesRoom(roomId, invitee);
                }
            } else {
                sendCallHangUp(roomId,
                               callId,
                               partyid_,
                               callPartyVersion_,
                               komai::voip::CallHangUpReason::InviteTimeOut);
                return;
            }
        }
    }

    if (ringtone != QLatin1String("Mute") && sharesRoom)
        playRingtone(ringtone == QLatin1String("Default")
                       ? QUrl(QStringLiteral("qrc:/media/media/ring.ogg"))
                       : QUrl::fromLocalFile(ringtone),
                     true);

    callParty_            = callerUserId;
    callPartyDisplayName_ = callerDisplayName;
    callPartyAvatarUrl_   = callerAvatarUrl;

    roomid_ = roomId;
    callid_ = callId;
    remoteICECandidates_.clear();

    haveCallInvite_ = true;
    callType_       = isVideo ? CallType::VIDEO : CallType::VOICE;
    inviteSDP_      = offerSdp;
    emit newInviteState();
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
    if (!preMatrixRtcCallsSettingEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    Q_UNUSED(roomId)
    Q_UNUSED(eventId)
    Q_UNUSED(version)

    if (senderId.toStdString() == utils::localUser().toStdString() && partyId == partyid_)
        return;
    komai::logging::ui()->debug("WebRTC: call id: {} - incoming CallCandidates from ({}, {})",
                                callId,
                                senderId.toStdString(),
                                partyId);

    if (callid_ == callId) {
        if (isOnCall()) {
            session_.acceptICECandidates(candidates);
        } else {
            for (const auto &candidate : candidates)
                remoteICECandidates_.push_back(candidate);
        }
    }
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
    if (!preMatrixRtcCallsSettingEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    Q_UNUSED(roomId)
    Q_UNUSED(eventId)
    Q_UNUSED(version)
    Q_UNUSED(answerType)

    komai::logging::ui()->debug("WebRTC: call id: {} - incoming CallAnswer from ({}, {})",
                                callId,
                                senderId.toStdString(),
                                partyId);
    if (answerSelected_)
        return;

    if (senderId.toStdString() == utils::localUser().toStdString() && callid_ == callId) {
        if (partyId == partyid_)
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

    if (isOnCall() && callid_ == callId) {
        stopRingtone();
        if (!session_.acceptAnswer(answerSdp)) {
            emit ChatPage::instance()->showNotification(QStringLiteral("Problem setting up call."));
            hangUp();
        }
    }
    sendCallSelectAnswer(roomid_, callid_, partyid_, callPartyVersion_, partyId);
    selectedpartyid_ = partyId;
    answerSelected_  = true;
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
    if (!preMatrixRtcCallsSettingEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    Q_UNUSED(roomId)
    Q_UNUSED(eventId)
    Q_UNUSED(version)

    const auto hangUpReason = hangUpReasonFromString(reason);
    komai::logging::ui()->debug("WebRTC: call id: {} - incoming CallHangUp ({}) from ({}, {})",
                                callId,
                                callHangUpReasonString(hangUpReason),
                                senderId.toStdString(),
                                partyId);

    if (callid_ == callId || isOnCallOnOtherDevice_ == callId)
        endCall();
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
    if (!preMatrixRtcCallsSettingEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    Q_UNUSED(roomId)
    Q_UNUSED(eventId)
    Q_UNUSED(version)

    komai::logging::ui()->debug("WebRTC: call id: {} - incoming CallSelectAnswer from ({}, {})",
                                callId,
                                senderId.toStdString(),
                                partyId);
    if (senderId.toStdString() == utils::localUser().toStdString()) {
        if (partyId != partyid_) {
            if (std::find(rejectCallPartyIDs_.begin(),
                          rejectCallPartyIDs_.end(),
                          selectedPartyId) != rejectCallPartyIDs_.end()) {
                endCall();
            } else {
                if (selectedPartyId == partyid_)
                    return;
                komai::logging::ui()->debug("WebRTC: call id: {} - user is on call with this user!",
                                            callId);
                isOnCallOnOtherDevice_ = callId;
                emit newCallDeviceState();
            }
        }
        return;
    } else if (callid_ == callId) {
        if (selectedPartyId != partyid_) {
            bool endAllCalls = false;
            if (std::find(rejectCallPartyIDs_.begin(),
                          rejectCallPartyIDs_.end(),
                          selectedPartyId) != rejectCallPartyIDs_.end()) {
                endAllCalls = true;
            } else {
                isOnCallOnOtherDevice_ = callid_;
                emit newCallDeviceState();
            }
            endCall(endAllCalls);
        } else if (session_.state() == webrtc::State::DISCONNECTED) {
            endCall();
        }
    }
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
    if (!preMatrixRtcCallsSettingEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    Q_UNUSED(roomId)
    Q_UNUSED(eventId)
    Q_UNUSED(version)

    komai::logging::ui()->debug("WebRTC: call id: {} - incoming CallReject from ({}, {})",
                                callId,
                                senderId.toStdString(),
                                partyId);
    if (answerSelected_)
        return;

    rejectCallPartyIDs_.push_back(partyId);
    if (senderId.toStdString() == utils::localUser().toStdString()) {
        if (partyId != partyid_ && callParty_ != utils::localUser())
            emit ChatPage::instance()->showNotification(
              QStringLiteral("Call rejected on another device."));
        endCall();
        return;
    }

    if (callId == callid_ && session_.state() == webrtc::State::OFFERSENT) {
        sendCallSelectAnswer(roomid_, callid_, partyid_, callPartyVersion_, partyId);
        endCall();
    }
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
    if (!preMatrixRtcCallsSettingEnabled())
        return;
#ifdef GSTREAMER_AVAILABLE
    Q_UNUSED(roomId)
    Q_UNUSED(senderId)
    Q_UNUSED(eventId)
    Q_UNUSED(lifetime)
    Q_UNUSED(descType)

    komai::logging::ui()->debug("WebRTC: call id: {} - incoming CallNegotiate from ({}, {})",
                                callId,
                                senderId.toStdString(),
                                partyId);

    if (!session_.acceptNegotiation(descSdp)) {
        emit ChatPage::instance()->showNotification(QStringLiteral("Problem accepting new SDP"));
        hangUp();
        return;
    }
    session_.acceptICECandidates(remoteICECandidates_);
    remoteICECandidates_.clear();
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
        hangUp(komai::voip::CallHangUpReason::UserMediaFailed);
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
        sendCallReject(roomid_, callid_, partyid_, callPartyVersion_);
    }
    if (!callid_.empty()) {
        komai::logging::ui()->debug("WebRTC: call id: {} - rejecting call", callid_);
        sendCallReject(roomid_, callid_, partyid_, callPartyVersion_);
        endCall(false);
    }
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
CallManager::preMatrixRtcCallsEnabled() const
{
    return preMatrixRtcCallsSettingEnabled();
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
    const auto handleId = activeMatrixRuntimeHandleId();
    if (handleId == 0) {
        komai::logging::ui()->warn(
          "Skipping TURN server retrieval because no matrix-sdk runtime handle "
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
          if (!isCurrentMatrixRuntimeHandle(result.handleId))
              return;

          if (!result.turnServerInfo) {
              komai::logging::ui()->warn("Failed to fetch TURN server info on handle {}: {}",
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
    komai::logging::ui()->debug("Trying to play ringtone {}", ringtone.toString().toStdString());
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
            komai::logging::ui()->error("Invalid TURN server uri: {}", uriString);
            continue;
        } else {
            std::string scheme = std::string(uriString, 0, c);
            if (scheme != "turn" && scheme != "turns") {
                komai::logging::ui()->error("Invalid TURN server uri: {}", uriString);
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
        komai::logging::net()->info("Homeserver returned no TURN server URIs");
    } else {
        komai::logging::net()->info("TURN server(s) retrieved from homeserver:");
        komai::logging::net()->info("username: {}", info.username.toStdString());
        komai::logging::net()->info("ttl: {} seconds", info.ttlSeconds);
        for (const auto &uri : info.uris)
            komai::logging::net()->info("uri: {}", uri.toStdString());
    }

    // Request new credentials close to expiry.
    turnURIs_ = getTurnURIs(info);

    uint64_t ttl = std::max(info.ttlSeconds, uint64_t{3600});
    if (!info.uris.isEmpty() && info.ttlSeconds < 3600)
        komai::logging::net()->warn("Setting ttl to 1 hour");
    turnServerTimer_.setInterval(std::chrono::seconds(ttl) * 10 / 9);
}

#include "moc_CallManager.cpp"
