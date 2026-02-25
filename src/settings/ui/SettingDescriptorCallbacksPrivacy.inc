// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static bool
privacyWindowFocusBlurDelaySecondsEnabled()
{
    auto i = I;
    return i && i->privacyWindowFocusBlurEnabled();
}
