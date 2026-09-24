// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QString>
#include <QtGlobal>

namespace komai::notificationPacing {

// In-memory per-room tracker suppressing repeat notifications within a pacing window.
// Not persisted; restarts clear all windows.
struct PacingTracker
{
    QHash<QString, qint64> lastDeliveredMs_;

    // pacingMinutes <= 0 disables pacing: never suppress.
    inline bool
    isSuppressed(const QString &roomId, qint64 nowMs, int pacingMinutes) const
    {
        if (pacingMinutes <= 0)
            return false;
        const auto it = lastDeliveredMs_.constFind(roomId);
        if (it == lastDeliveredMs_.constEnd())
            return false;
        return nowMs - it.value() < pacingMinutes * 60 * 1000;
    }

    // Call once per delivered notification; starts or extends the window.
    inline void
    recordDelivered(const QString &roomId, qint64 nowMs)
    {
        lastDeliveredMs_[roomId] = nowMs;
    }

    // Reset one room's window (user opened/read that room); no-op if missing.
    inline void
    clearRoom(const QString &roomId)
    {
        lastDeliveredMs_.remove(roomId);
    }

    // Drop all state (logout).
    inline void
    clear()
    {
        lastDeliveredMs_.clear();
    }
};

} // namespace komai::notificationPacing
