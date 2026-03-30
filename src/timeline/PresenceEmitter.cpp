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

void
PresenceEmitter::sync(const QVector<QString> &userIds)
{
    for (const auto &id : userIds) {
        presences.remove(id);
        emit presenceChanged(id);
    }
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

#include "moc_PresenceEmitter.cpp"
