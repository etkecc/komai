// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PresenceEmitter.h"

#include <QCache>

namespace {
struct CacheEntry
{
    QString status;
    QString state;
};
}

static QCache<QString, CacheEntry> presences;

static CacheEntry *
pullPresence(const QString &id)
{
    auto *entry = new CacheEntry{QString{}, QStringLiteral("offline")};
    presences.insert(id, entry);
    return entry;
}

QString
PresenceEmitter::userPresence(QString id) const
{
    if (id.isEmpty())
        return {};
    if (auto *presence = presences[id])
        return presence->state;
    return pullPresence(id)->state;
}

QString
PresenceEmitter::userStatus(QString id) const
{
    if (id.isEmpty())
        return {};
    if (auto *presence = presences[id])
        return presence->status;
    return pullPresence(id)->status;
}

void
PresenceEmitter::setLocalPresence(const QString &userId,
                                  const QString &state,
                                  const QString &status)
{
    const auto trimmedUserId = userId.trimmed();
    if (trimmedUserId.isEmpty())
        return;

    const auto normalizedState =
      state.trimmed().isEmpty() ? QStringLiteral("offline") : state.trimmed();
    auto *presence = presences[trimmedUserId];
    if (!presence)
        presence = pullPresence(trimmedUserId);

    const bool changed = presence->state != normalizedState || presence->status != status;
    if (!changed)
        return;

    presence->state  = normalizedState;
    presence->status = status;
    emit presenceChanged(trimmedUserId);
}

void
PresenceEmitter::setLocalStatus(const QString &userId, const QString &status)
{
    const auto trimmedUserId = userId.trimmed();
    if (trimmedUserId.isEmpty())
        return;

    const auto *presence = presences[trimmedUserId];
    setLocalPresence(trimmedUserId, presence ? presence->state : QStringLiteral("offline"), status);
}

void
PresenceEmitter::clear()
{
    if (presences.isEmpty())
        return;

    const auto keys = presences.keys();
    presences.clear();

    for (const auto &userId : keys)
        emit presenceChanged(userId);
}

#include "moc_PresenceEmitter.cpp"
