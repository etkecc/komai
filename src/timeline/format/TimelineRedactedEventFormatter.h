// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QVariantMap>

#include <functional>

class EventStore;
class QString;

namespace timeline::format {
using DisplayNameForUserFn = std::function<QString(const QString &)>;

QVariantMap
formatRedactedEvent(const QString &id,
                    EventStore &eventStore,
                    const DisplayNameForUserFn &displayNameForUser);
}
