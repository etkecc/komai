// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQml

// Single source of truth for the two-tier muted-text treatment used on
// row-like surfaces. Both the room-list rows and the bubble metadata
// row sit on a background (window or base) and need a dim variant of
// the primary text color for second-line content. Centralising the
// blend factors here keeps the two surfaces consistent: a tweak made
// for legibility on one immediately applies to the other.
//
// Derivation: blend palette.text toward the surrounding background by
// `blend`. The room list uses `palette.window`; the bubble metadata
// uses the chat-area `palette.base`. The choice not to use
// `palette.buttonText` is deliberate -- that role is for actual button
// labels, and theme authors may set it equal to `palette.text`, which
// would flatten the row hierarchy.
//
// WCAG: blend factors below were picked so the derived colors stay at
// or above the corresponding WCAG 2.0 contrast minimum across every
// bundled theme. Pushing factors higher than these falls below the
// worst-case theme's minimum, so do not raise them without re-running
// the contrast survey across the themes in `resources/themes/*.yml`.
QtObject {
    // For important secondary content (e.g. last-message preview).
    // Result stays at WCAG AA-normal (>=4.5:1) on every bundled theme;
    // the floor is ~4.68:1 (light-rose-pine-dawn).
    readonly property real previewBlend: 0.15

    // For peripheral metadata (e.g. timestamps). Result stays at
    // WCAG AA-large (>=3:1) on every bundled theme; floor is ~3.74:1
    // (light-rose-pine-dawn). Most themes land at AA-normal here too.
    readonly property real timestampBlend: 0.25

    function muted(textColor, bgColor, blend) {
        return Qt.rgba(textColor.r * (1 - blend) + bgColor.r * blend,
                       textColor.g * (1 - blend) + bgColor.g * blend,
                       textColor.b * (1 - blend) + bgColor.b * blend,
                       1);
    }
}
