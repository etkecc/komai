// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <mtx/events/collections.hpp>
#include <mtx/events/power_levels.hpp>

#include <functional>

class EventStore;

namespace timeline::format {
using DisplayNameForUserFn = std::function<QString(const QString &)>;

QString
formatPowerLevelEvent(const mtx::events::StateEvent<mtx::events::state::PowerLevels> &event,
                      EventStore &eventStore,
                      const DisplayNameForUserFn &displayNameForUser);
}
