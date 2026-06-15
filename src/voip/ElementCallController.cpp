// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ElementCallController.h"

#include "logging/Logging.h"

ElementCallController::ElementCallController(QObject *parent)
  : QObject(parent)
{
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
