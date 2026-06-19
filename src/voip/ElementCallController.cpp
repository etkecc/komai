// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ElementCallController.h"

#include <QAudioOutput>
#include <QDateTime>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>

#include "chat/ChatPage.h"
#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"

ElementCallController *ElementCallController::s_instance_ = nullptr;

ElementCallController::ElementCallController(QObject *parent)
  : QObject(parent)
{
    s_instance_ = this;
}

bool
ElementCallController::supported()
{
#ifdef ELEMENT_CALL_AVAILABLE
    return true;
#else
    return false;
#endif
}

void
ElementCallController::startCall(const QString &roomId)
{
    if (!supported())
        return;
    if (roomId.trimmed().isEmpty()) {
        komai::logging::ui()->warn("[EC] startCall ignored: empty room id");
        return;
    }
    if (active_) {
        if (activeRoomId_ == roomId)
            return; // Already in this call.
        komai::logging::ui()->warn(
          "[EC] startCall ignored: a call is already active in another room");
        return;
    }

    // Joining the call answers any incoming ring for that room.
    if (ringActive_ && ringRoomId_ == roomId) {
        handledNotifications_.insert(ringNotificationEventId_);
        clearRing();
    }

    // Joining clears any desktop notification(s) for this room's call (the ring
    // one is closed by clearRing above; a group one is closed here).
    if (auto *chat = ChatPage::instance())
        chat->withdrawCallNotificationsForRoom(roomId);

    activeRoomId_ = roomId;
    active_       = true;
    emit activeRoomIdChanged();
    emit activeChanged();
    komai::logging::ui()->warn("[EC] call requested for room {}", roomId.toStdString());
}

void
ElementCallController::hangup()
{
    if (!active_)
        return;
    // The panel owns the session; ask it to leave gracefully. It calls
    // notifyStopped() once the teardown completes.
    emit hangupRequested();
}

void
ElementCallController::notifyStopped()
{
    if (!active_ && activeRoomId_.isEmpty())
        return;
    active_ = false;
    activeRoomId_.clear();
    emit activeRoomIdChanged();
    emit activeChanged();
    komai::logging::ui()->warn("[EC] call surface stopped");
}

void
ElementCallController::onRtcNotification(const QString &roomId,
                                         const QString &notificationEventId,
                                         const QString &senderId,
                                         const QString &notificationType,
                                         bool isSelf,
                                         bool mentionsMe,
                                         quint64 expiresAtMs,
                                         int notificationMode)
{
    // Two kinds of incoming MatrixRTC notification reach us here:
    //   * `ring`         — an addressed 1:1 invite. We RING (in-app ring bar +
    //                      ringtone) and raise a desktop notification.
    //   * `notification` — a silent group-call notice. No ring; we raise a
    //                      silent desktop notification honouring the room's
    //                      notify setting. Also surfaced by the timeline tile /
    //                      avatar glow.
    if (!supported())
        return;

    const bool isRing = (notificationType == QStringLiteral("ring"));
    if (!isRing && notificationType != QStringLiteral("notification"))
        return; // Unknown notification type.
    if (isSelf)
        return;
    if (active_ && activeRoomId_ == roomId)
        return; // We are already in this call.
    if (handledNotifications_.contains(notificationEventId))
        return; // Declined / dismissed / already joined.

    const quint64 now = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    if (expiresAtMs != 0 && expiresAtMs <= now)
        return; // The notification already expired before we processed it.

    // Ring: only when we are actually addressed (a ring not aimed at us is not
    // ours to answer). Drives the global ring bar + ringtone.
    if (isRing && mentionsMe) {
        ringRoomId_              = roomId;
        ringNotificationEventId_ = notificationEventId;
        ringSenderId_            = senderId;
        ringActive_              = true;
        ringWasLive_             = false;

        // Stop ringing when the notification expires (MSC4075 lifetime), matching
        // how long the caller rings out. Fall back to a 30s ring if no expiry was
        // given.
        if (!ringExpiryTimer_) {
            ringExpiryTimer_ = new QTimer(this);
            ringExpiryTimer_->setSingleShot(true);
            connect(ringExpiryTimer_, &QTimer::timeout, this, [this]() {
                komai::logging::ui()->warn("[EC] incoming ring expired");
                clearRing();
            });
        }
        const quint64 remaining = (expiresAtMs > now) ? (expiresAtMs - now) : 30000;
        ringExpiryTimer_->start(static_cast<int>(qMin<quint64>(remaining, 120000)));

        startRingtone();
        emit incomingRingChanged();
        komai::logging::ui()->warn("[EC] incoming call ring from {} in room {}",
                                   senderId.toStdString(),
                                   roomId.toStdString());
    }

    // A ring not addressed to us is neither rung nor notified.
    if (isRing && !mentionsMe)
        return;

    maybePostCallNotification(
      roomId, notificationEventId, senderId, isRing, mentionsMe, notificationMode);
}

void
ElementCallController::maybePostCallNotification(const QString &roomId,
                                                 const QString &notificationEventId,
                                                 const QString &senderId,
                                                 bool isRing,
                                                 bool mentionsMe,
                                                 int notificationMode)
{
    // Silent group notifications honour the room's notify setting the same way a
    // regular message would: skip muted rooms, and in mentions-only rooms notify
    // only when we are addressed (a personal mention OR a room-wide @room, which
    // also notifies for messages via the roomnotif push rule). Ring calls are
    // always eligible (an addressed 1:1 invite).
    if (!isRing) {
        if (notificationMode == 0 /* mute */)
            return;
        if (notificationMode == 1 /* mentions / keywords only */ && !mentionsMe)
            return;
    }

    if (postedCallNotifications_.contains(notificationEventId))
        return; // Already raised a desktop notification for this call.
    postedCallNotifications_.insert(notificationEventId);

    // ChatPage owns the notification surface (and applies focus gating). It is
    // always present once a session is up, which is the only time RTC
    // notifications arrive; if it is somehow absent there is nothing to show.
    if (auto *chat = ChatPage::instance())
        chat->dispatchCallNotification(
          roomId, notificationEventId, senderId, isRing, /*canDecline=*/isRing);
}

void
ElementCallController::onRtcDecline(const QString &notificationEventId, bool isSelf)
{
    // Only our OWN decline (e.g. from another device) silences us; other
    // participants declining a group ring does not.
    if (!isSelf)
        return;
    handledNotifications_.insert(notificationEventId);
    if (ringActive_ && ringNotificationEventId_ == notificationEventId) {
        komai::logging::ui()->warn("[EC] incoming ring declined on another device");
        clearRing();
    }
}

void
ElementCallController::declineIncomingRing()
{
    if (!ringActive_)
        return;

    const std::string roomIdStd = ringRoomId_.toStdString();
    const std::string notifStd  = ringNotificationEventId_.toStdString();
    handledNotifications_.insert(ringNotificationEventId_);

    auto *mainWindow       = MainWindow::instance();
    const quint64 handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId != 0) {
        try {
            ::komai::rust::matrix_element_call_decline(handleId, roomIdStd, notifStd);
        } catch (const std::exception &e) {
            komai::logging::ui()->warn("[EC] failed to send m.rtc.decline: {}", e.what());
        }
    }

    // The ring bar disappearing + the ringtone stopping are clear enough feedback
    // that the decline registered; no toast needed.
    clearRing();
}

void
ElementCallController::dismissIncomingRing()
{
    if (!ringActive_)
        return;
    handledNotifications_.insert(ringNotificationEventId_);
    clearRing();
}

void
ElementCallController::updateRingLiveness(bool live)
{
    if (!ringActive_)
        return;
    if (live) {
        ringWasLive_ = true;
        return;
    }
    // Not live: only treat this as a cancellation once we have confirmed the call
    // was live, so we don't kill the ring during the brief window before the
    // caller's call.member reaches our room-list snapshot.
    if (ringWasLive_) {
        komai::logging::ui()->warn("[EC] incoming call ended before answer");
        handledNotifications_.insert(ringNotificationEventId_);
        clearRing();
    }
}

void
ElementCallController::startRingtone()
{
    const QString ringtone = UserSettings::instance()->callsAudioRingtone();
    if (ringtone == QLatin1String("Mute"))
        return; // Honour the shared ringtone setting.

    const QUrl source = (ringtone.isEmpty() || ringtone == QLatin1String("Default"))
                          ? QUrl(QStringLiteral("qrc:/media/media/ring.ogg"))
                          : QUrl::fromLocalFile(ringtone);

    if (!ringPlayer_) {
        ringPlayer_ = new QMediaPlayer(this);
        ringAudio_  = new QAudioOutput(this);
        ringPlayer_->setAudioOutput(ringAudio_);
        ringPlayer_->setLoops(QMediaPlayer::Infinite);
    }
    ringPlayer_->setSource(source);
    ringPlayer_->play();
}

void
ElementCallController::stopRingtone()
{
    if (ringPlayer_)
        ringPlayer_->stop();
}

void
ElementCallController::clearRing()
{
    stopRingtone();
    if (ringExpiryTimer_)
        ringExpiryTimer_->stop();
    if (!ringActive_)
        return;
    // Close the desktop notification raised for this ring (no-op if none / if it
    // was suppressed because the app was focused).
    if (auto *chat = ChatPage::instance())
        chat->withdrawCallNotification(ringRoomId_, ringNotificationEventId_);
    ringActive_  = false;
    ringWasLive_ = false;
    ringRoomId_.clear();
    ringNotificationEventId_.clear();
    ringSenderId_.clear();
    emit incomingRingChanged();
}
