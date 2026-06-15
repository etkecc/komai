// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

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

public:
    explicit ElementCallController(QObject *parent = nullptr);

    static bool supported();
    bool active() const { return active_; }
    QString activeRoomId() const { return activeRoomId_; }

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

signals:
    void activeChanged();
    void activeRoomIdChanged();
    // The panel should run Element Call's graceful leave flow for the active call.
    void hangupRequested();

private:
    bool active_ = false;
    QString activeRoomId_;
};
