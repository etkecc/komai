// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Throwaway QtWebEngine build spike (M2 of the Element Call work). It only
// proves that WebEngineView compiles, links, and renders on native builds; it
// is reachable via the Ctrl+Alt+E shortcut wired in shell/Root.qml and will be
// replaced by the real call surface in a later milestone.

import QtQuick
import QtQuick.Window
import QtWebEngine

Window {
    id: spikeWindow

    width: 1024
    height: 768
    title: qsTr("Element Call build spike")

    WebEngineView {
        anchors.fill: parent
        url: "https://example.org"
    }
}
