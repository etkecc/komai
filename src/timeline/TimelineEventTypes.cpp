// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineEventTypes.h"

std::vector<qml_mtx_events::EventType>
qml_mtx_events::defaultHiddenEventTypes()
{
    return {
      qml_mtx_events::Reaction,
      qml_mtx_events::CallCandidates,
      qml_mtx_events::CallNegotiate,
      qml_mtx_events::Unsupported,
      qml_mtx_events::CallSelectAnswer,
    };
}
