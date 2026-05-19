// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Drop inside a Flickable-derived item (ListView, Flickable, GridView, …)
// to make its mouse-wheel scrolling feel comfortable. Qt's default
// Flickable handler only moves ~60px per wheel notch, which feels
// sluggish on long delegates or settings rows. `ScrollView` ships its
// own faster wheel handling; reach for this booster when a bare
// Flickable/ListView with a custom ScrollBar can't easily be replaced
// with a ScrollView.
//
// Usage — pass the host explicitly:
//
//     ListView {
//         id: rooms
//         FlickableWheelBooster { flickable: rooms }
//     }
//
// `parent`-based defaulting was tried but proved unreliable: when a QML
// component file extends `WheelHandler`, the implicit parent that gets
// assigned at instantiation differs from the visual parent that the
// inline pattern resolves to, and bindings on `parent` don't always
// re-evaluate as the handler is attached. Explicit `flickable:` keeps
// this boring and predictable.
import QtQuick

WheelHandler {
    id: root

    required property var flickable

    // Per-notch travel multiplier. The timeline uses the same value via
    // its own pin-aware handler in MatrixRoomListShellSupport.
    property real multiplier: 5

    orientation: Qt.Vertical
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

    property real _prevRotation: 0
    onRotationChanged: {
        const delta = rotation - _prevRotation;
        _prevRotation = rotation;
        if (!flickable)
            return;
        const range = Math.max(0, flickable.contentHeight - flickable.height);
        if (range <= 0)
            return;
        // Use originY rather than 0 as the floor so bottom-to-top
        // ListViews (e.g. the pinned-messages dialog) are clamped to
        // their actual content range, not a stale "starts at zero"
        // assumption.
        const minY = flickable.originY;
        const maxY = flickable.originY + range;
        const proposedY = flickable.contentY - delta * multiplier;
        flickable.contentY = Math.max(minY, Math.min(maxY, proposedY));
    }
}
