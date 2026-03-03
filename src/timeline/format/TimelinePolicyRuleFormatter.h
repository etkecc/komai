// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <mtx/events/collections.hpp>

#include <functional>

class EventStore;

namespace timeline::format {
using DisplayNameForUserFn = std::function<QString(const QString &)>;

QString
formatPolicyRule(const QString &id,
                 EventStore &eventStore,
                 const DisplayNameForUserFn &displayNameForUser);
}
