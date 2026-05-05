// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <QMediaPlayer>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "CallDevices.h"
#include "CallTypes.h"
#include "WebRTCSession.h"
#include "voip/ScreenCastPortal.h"

namespace komai {
struct MatrixTurnServerInfo;
}

class QUrl;

class CallManager : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool haveCallInvite READ haveCallInvite NOTIFY newInviteState)
    Q_PROPERTY(bool isOnCall READ isOnCall NOTIFY newCallState)
    Q_PROPERTY(bool isOnCallOnOtherDevice READ isOnCallOnOtherDevice NOTIFY newCallDeviceState)
    Q_PROPERTY(webrtc::CallType callType READ callType NOTIFY newInviteState)
    Q_PROPERTY(
      webrtc::ScreenShareType screenShareType READ screenShareType NOTIFY screenShareChanged)
    Q_PROPERTY(webrtc::State callState READ callState NOTIFY newCallState)
    Q_PROPERTY(QString callRoomId READ callRoomId NOTIFY newInviteState)
    Q_PROPERTY(QString callParty READ callParty NOTIFY newInviteState)
    Q_PROPERTY(QString callPartyDisplayName READ callPartyDisplayName NOTIFY newInviteState)
    Q_PROPERTY(QString callPartyAvatarUrl READ callPartyAvatarUrl NOTIFY newInviteState)
    Q_PROPERTY(bool isMicMuted READ isMicMuted NOTIFY micMuteChanged)
    Q_PROPERTY(bool haveLocalPiP READ haveLocalPiP NOTIFY newCallState)
    Q_PROPERTY(QStringList mics READ mics NOTIFY devicesChanged)
    Q_PROPERTY(QStringList cameras READ cameras NOTIFY devicesChanged)
    Q_PROPERTY(bool callsSupported READ callsSupported CONSTANT)
    Q_PROPERTY(bool preMatrixRtcCallsEnabled READ preMatrixRtcCallsEnabled NOTIFY
                 preMatrixRtcCallsEnabledChanged)
    Q_PROPERTY(bool screenShareReady READ screenShareReady NOTIFY screenShareChanged)

public:
    CallManager(QObject *);

    static CallManager *create(QQmlEngine *qmlEngine, QJSEngine *);

    bool haveCallInvite() const { return haveCallInvite_; }
    bool isOnCall() const { return (session_.state() != webrtc::State::DISCONNECTED); }
    bool isOnCallOnOtherDevice() const { return (isOnCallOnOtherDevice_ != ""); }
    bool checkSharesRoom(QString roomid_, std::string invitee) const;
    webrtc::CallType callType() const { return callType_; }
    webrtc::ScreenShareType screenShareType() const { return screenShareType_; }
    webrtc::State callState() const { return session_.state(); }
    QString callRoomId() const { return roomid_; }
    QString callParty() const { return callParty_; }
    QString callPartyDisplayName() const { return callPartyDisplayName_; }
    QString callPartyAvatarUrl() const { return callPartyAvatarUrl_; }
    bool isMicMuted() const { return session_.isMicMuted(); }
    bool haveLocalPiP() const { return session_.haveLocalPiP(); }
    QStringList mics() const { return devices(false); }
    QStringList cameras() const { return devices(true); }
    bool preMatrixRtcCallsEnabled() const;
    void refreshTurnServer();
    bool screenShareReady() const;

    static bool callsSupported();

public slots:
    void sendInvite(const QString &roomid, webrtc::CallType, unsigned int windowIndex = 0);

    void handleCallInvite(const QString &roomId,
                          const QString &senderId,
                          const QString &eventId,
                          const std::string &callId,
                          const std::string &partyId,
                          const std::string &version,
                          uint32_t lifetime,
                          const std::string &invitee,
                          const std::string &offerSdp,
                          const std::string &offerType);

    void handleCallCandidates(const QString &roomId,
                              const QString &senderId,
                              const QString &eventId,
                              const std::string &callId,
                              const std::string &partyId,
                              const std::string &version,
                              const komai::voip::CallIceCandidateList &candidates);

    void handleCallAnswer(const QString &roomId,
                          const QString &senderId,
                          const QString &eventId,
                          const std::string &callId,
                          const std::string &partyId,
                          const std::string &version,
                          const std::string &answerSdp,
                          const std::string &answerType);

    void handleCallHangUp(const QString &roomId,
                          const QString &senderId,
                          const QString &eventId,
                          const std::string &callId,
                          const std::string &partyId,
                          const std::string &version,
                          const std::string &reason);

    void handleCallSelectAnswer(const QString &roomId,
                                const QString &senderId,
                                const QString &eventId,
                                const std::string &callId,
                                const std::string &partyId,
                                const std::string &version,
                                const std::string &selectedPartyId);

    void handleCallReject(const QString &roomId,
                          const QString &senderId,
                          const QString &eventId,
                          const std::string &callId,
                          const std::string &partyId,
                          const std::string &version);

    void handleCallNegotiate(const QString &roomId,
                             const QString &senderId,
                             const QString &eventId,
                             const std::string &callId,
                             const std::string &partyId,
                             uint32_t lifetime,
                             const std::string &descSdp,
                             const std::string &descType);
    void toggleMicMute();
    void toggleLocalPiP() { session_.toggleLocalPiP(); }
    void acceptInvite();
    void hangUp(komai::voip::CallHangUpReason reason = komai::voip::CallHangUpReason::UserHangUp);
    void rejectInvite();
    void setupScreenShareXDP();
    void setScreenShareType(unsigned int index);
    void closeScreenShare();
    QStringList screenShareTypeList();
    QStringList windowList();
    void previewWindow(unsigned int windowIndex) const;

signals:
    void newInviteState();
    void newCallState();
    void newCallDeviceState();
    void micMuteChanged();
    void devicesChanged();
    void preMatrixRtcCallsEnabledChanged();
    void screenShareChanged();

private slots:
    void retrieveTurnServer();

private:
    static bool screenShareUsesWindowPicker(webrtc::ScreenShareType type);

    void sendCallInvite(const QString &roomId,
                        const std::string &callId,
                        const std::string &partyId,
                        const std::string &version,
                        uint32_t lifetime,
                        const std::string &invitee,
                        const std::string &offerSdp,
                        komai::voip::CallSdpType offerType);
    void sendCallCandidates(const QString &roomId,
                            const std::string &callId,
                            const std::string &partyId,
                            const komai::voip::CallIceCandidateList &candidates,
                            const std::string &version);
    void sendCallAnswer(const QString &roomId,
                        const std::string &callId,
                        const std::string &partyId,
                        const std::string &version,
                        const std::string &answerSdp,
                        komai::voip::CallSdpType answerType);
    void sendCallHangUp(const QString &roomId,
                        const std::string &callId,
                        const std::string &partyId,
                        const std::string &version,
                        komai::voip::CallHangUpReason reason);
    void sendCallSelectAnswer(const QString &roomId,
                              const std::string &callId,
                              const std::string &partyId,
                              const std::string &version,
                              const std::string &selectedPartyId);
    void sendCallReject(const QString &roomId,
                        const std::string &callId,
                        const std::string &partyId,
                        const std::string &version);
    void sendCallNegotiate(const QString &roomId,
                           const std::string &callId,
                           const std::string &partyId,
                           uint32_t lifetime,
                           const std::string &descSdp,
                           komai::voip::CallSdpType descType);

    WebRTCSession &session_;
    QString roomid_;
    QString callParty_;
    QString callPartyDisplayName_;
    QString callPartyAvatarUrl_;
    std::string callPartyVersion_ = "1";
    std::string callid_;
    std::string partyid_;
    std::string selectedpartyid_             = "";
    std::string invitee_                     = "";
    const uint32_t timeoutms_                = 120000;
    webrtc::CallType callType_               = webrtc::CallType::VOICE;
    webrtc::ScreenShareType screenShareType_ = webrtc::ScreenShareType::X11;
    bool haveCallInvite_                     = false;
    bool answerSelected_                     = false;
    std::string isOnCallOnOtherDevice_       = "";
    std::string inviteSDP_;
    komai::voip::CallIceCandidateList remoteICECandidates_;
    std::vector<std::string> turnURIs_;
    QTimer turnServerTimer_;
    std::once_flag playerInitOnce_;
    std::unique_ptr<QMediaPlayer> player_;
    std::vector<webrtc::ScreenShareType> screenShareTypes_;
#ifndef Q_OS_WINDOWS
    std::vector<std::pair<QString, uint32_t>> windows_;
#else
    std::vector<std::pair<QString, uint64_t>> windows_;
#endif
    std::vector<std::string> rejectCallPartyIDs_;
    void generateCallID();
    QStringList devices(bool isVideo) const;
    void clear(bool endAllCalls = true);
    void endCall(bool endAllCalls = true);
    QMediaPlayer *ensurePlayerInitialized();
    void playRingtone(const QUrl &ringtone, bool repeat);
    void stopRingtone();
    void applyTurnServerInfo(const komai::MatrixTurnServerInfo &info);
};
