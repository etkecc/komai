// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingDescriptorCallbacksCallsDevices.inc"
#include "settings/ui/SettingDescriptorCallbacksCallsRingtone.inc"

// Gates the "Legacy Calls" subsection rows: their controls (microphone, camera,
// screen sharing, fallback relay) drive only the legacy GStreamer pipeline, so
// they grey out when legacy calls are disabled. Element Call enumerates and
// selects its own devices inside the in-call interface and consumes none of them.
static bool
callsLegacyConfigEnabled()
{
    auto i = I;
    return i && i->callsLegacyEnabled();
}
