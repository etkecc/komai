// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>

class QMediaPlayer;
class QAudioOutput;
class QTimer;

// App-global coordinator for the in-room Element Call surface.
//
// This is the always-compiled hand-off point between the composer call button
// (and any other entry point) and the Element Call panel that hosts the
// QtWebEngine widget. It deliberately holds NO QtWebEngine / widget-driver
// state of its own: it only tracks which room has the active call so the
// composer button and the panel can coordinate, exactly the way the legacy
// CallManager singleton exposes call state to QML. The actual webview + Matrix
// widget driver live in ElementCallPanel.qml / ElementCallWidgetSession, which
// are only built when ELEMENT_CALL is enabled.
//
// Flow:
//   1. The composer button calls startCall(roomId). We record the room and flip
//      `active`, which makes the panel's Loader (active: ElementCall.active)
//      instantiate the panel.
//   2. The panel observes activeRoomId, starts a widget session for it and shows
//      itself in that room's timeline view (hidden while another room is open).
//   3. hangup() asks the panel to leave gracefully (hangupRequested); the panel
//      tears the session down and calls notifyStopped(), which clears the state
//      and unloads the panel.
class ElementCallController : public QObject
{
    Q_OBJECT

    QML_NAMED_ELEMENT(ElementCall)
    QML_SINGLETON

    // Whether Element Call support was compiled in (QtWebEngine available). False
    // on -DELEMENT_CALL=OFF builds, where the call surface does not exist.
    Q_PROPERTY(bool supported READ supported CONSTANT)
    // Whether a call surface is currently up (a session is starting or running).
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    // The room the active call belongs to (empty when no call is up).
    Q_PROPERTY(QString activeRoomId READ activeRoomId NOTIFY activeRoomIdChanged)

    // An incoming MatrixRTC call is ringing us (a `ring` m.rtc.notification we are
    // addressed by, that we have neither joined nor declined). Drives the global
    // incoming-call ring bar.
    Q_PROPERTY(bool incomingRingActive READ incomingRingActive NOTIFY incomingRingChanged)
    // The room the incoming ring belongs to (empty when not ringing).
    Q_PROPERTY(QString incomingRingRoomId READ incomingRingRoomId NOTIFY incomingRingChanged)
    // The user who started the ringing call (empty when not ringing).
    Q_PROPERTY(QString incomingRingSenderId READ incomingRingSenderId NOTIFY incomingRingChanged)

public:
    explicit ElementCallController(QObject *parent = nullptr);

    // The single QML-created instance, so the Matrix backend bridge can route
    // MatrixRTC ring/decline callbacks here. May be null very early in startup.
    static ElementCallController *instance() { return s_instance_; }

    static bool supported();
    bool active() const { return active_; }
    QString activeRoomId() const { return activeRoomId_; }

    bool incomingRingActive() const { return ringActive_; }
    QString incomingRingRoomId() const { return ringRoomId_; }
    QString incomingRingSenderId() const { return ringSenderId_; }

    // Starts (or focuses) an Element Call in roomId. No-op when unsupported, when
    // roomId is empty, or when a call is already up in a different room (one call
    // at a time, matching the legacy stack).
    Q_INVOKABLE void startCall(const QString &roomId);

    // Asks the active call surface to leave gracefully. The panel runs Element
    // Call's leave flow and then calls notifyStopped().
    Q_INVOKABLE void hangup();

    // Called by the panel once the call surface has fully torn down, so the
    // controller can clear its state and let the Loader unload the panel.
    Q_INVOKABLE void notifyStopped();

    // Decline the current incoming ring: send an m.rtc.decline (stops the ring on
    // our other devices and tells a DM caller to stop ringing out), silence the
    // ringtone, and remember it so it does not ring again.
    Q_INVOKABLE void declineIncomingRing();

    // Silence + clear the current incoming ring WITHOUT declining. Used when the
    // call ends on its own (the room left Rooms.activeCalls) or once we join it.
    Q_INVOKABLE void dismissIncomingRing();

    // Feed whether the ringing call is still live (someone is in it), sourced
    // from Rooms.activeCalls in QML. Once we have seen it live, a transition to
    // not-live means the caller cancelled before we answered, so we stop ringing.
    // Gating on "seen live first" avoids racing the initial call.member sync,
    // where activeCalls may briefly lack the room when the ring arrives.
    Q_INVOKABLE void updateRingLiveness(bool live);

    // MatrixRTC callbacks, invoked on the GUI thread by the Matrix backend bridge.
    void onRtcNotification(const QString &roomId,
                           const QString &notificationEventId,
                           const QString &senderId,
                           const QString &notificationType,
                           bool isSelf,
                           bool mentionsMe,
                           quint64 expiresAtMs,
                           int notificationMode);
    void onRtcDecline(const QString &notificationEventId, bool isSelf);

signals:
    void activeChanged();
    void activeRoomIdChanged();
    // The panel should run Element Call's graceful leave flow for the active call.
    void hangupRequested();

    void incomingRingChanged();

private:
    void startRingtone();
    void stopRingtone();
    // Silence the ringtone, stop the expiry timer, and clear the ring state.
    void clearRing();
    // Decide whether an incoming RTC notification should raise a desktop OS
    // notification and, if so, hand it to ChatPage to present. Ring calls are
    // always eligible (they are addressed 1:1 invites); silent group
    // notifications honour the room's notify setting (mute / mentions-only).
    void maybePostCallNotification(const QString &roomId,
                                   const QString &notificationEventId,
                                   const QString &senderId,
                                   bool isRing,
                                   bool mentionsMe,
                                   int notificationMode);

    static ElementCallController *s_instance_;

    bool active_ = false;
    QString activeRoomId_;

    bool ringActive_ = false;
    // Whether the ringing call has been observed live in Rooms.activeCalls at
    // least once during this ring (see updateRingLiveness).
    bool ringWasLive_ = false;
    QString ringRoomId_;
    QString ringNotificationEventId_;
    QString ringSenderId_;
    QTimer *ringExpiryTimer_  = nullptr;
    QMediaPlayer *ringPlayer_ = nullptr;
    QAudioOutput *ringAudio_  = nullptr;
    // Notification event ids we have declined / dismissed, so a re-delivered or
    // duplicate notification does not ring again.
    QSet<QString> handledNotifications_;
    // Notification event ids we have already raised a desktop notification for,
    // so a re-delivered notification does not pop a second one.
    QSet<QString> postedCallNotifications_;
};
