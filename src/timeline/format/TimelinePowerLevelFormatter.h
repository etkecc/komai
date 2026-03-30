// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <functional>

#include "matrix/MatrixPowerLevelCompat.h"

class EventStore;

namespace timeline::format {
using DisplayNameForUserFn = std::function<QString(const QString &)>;

QString
formatPowerLevelEvent(const mtx::events::StateEvent<mtx::events::state::PowerLevels> &event,
                      const mtx::events::StateEvent<mtx::events::state::Create> &create,
                      EventStore &eventStore,
                      const DisplayNameForUserFn &displayNameForUser);
}
