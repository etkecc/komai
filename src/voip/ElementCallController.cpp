// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ElementCallController.h"

#include <QAudioOutput>
#include <QDateTime>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>

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
                                         quint64 expiresAtMs)
{
    // We only RING for `ring` notifications addressed to us, from someone else,
    // for a call we have neither joined nor already handled. Silent
    // `notification` (group) notifications are surfaced by the timeline tile /
    // avatar glow, not here.
    if (!supported())
        return;
    if (notificationType != QStringLiteral("ring"))
        return;
    if (isSelf || !mentionsMe)
        return;
    if (active_ && activeRoomId_ == roomId)
        return;
    if (handledNotifications_.contains(notificationEventId))
        return;

    const quint64 now = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    if (expiresAtMs != 0 && expiresAtMs <= now)
        return; // The ring already expired before we processed it.

    ringRoomId_              = roomId;
    ringNotificationEventId_ = notificationEventId;
    ringSenderId_            = senderId;
    ringActive_              = true;
    ringWasLive_             = false;

    // Stop ringing when the notification expires (MSC4075 lifetime), matching how
    // long the caller rings out. Fall back to a 30s ring if no expiry was given.
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
    komai::logging::ui()->warn(
      "[EC] incoming call ring from {} in room {}", senderId.toStdString(), roomId.toStdString());
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
    ringActive_  = false;
    ringWasLive_ = false;
    ringRoomId_.clear();
    ringNotificationEventId_.clear();
    ringSenderId_.clear();
    emit incomingRingChanged();
}
