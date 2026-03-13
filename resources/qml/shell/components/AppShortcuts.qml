// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Item {
    required property var componentCatalog
    required property var timelineRoot

    Shortcut {
        sequence: StandardKey.Quit

        onActivated: Qt.quit()
    }
    Shortcut {
        sequences: ["Escape"]
        context: Qt.ApplicationShortcut
        enabled: !!timelineRoot.activeMediaOverlay && timelineRoot.activeMediaOverlay.visible

        onActivated: timelineRoot.activeMediaOverlay.close()
        onActivatedAmbiguously: timelineRoot.activeMediaOverlay.close()
    }
    Shortcut {
        sequences: ["Ctrl+K", "Ctrl+P"]

        onActivated: timelineRoot.openCatalogDialog(componentCatalog.navigationQuickSwitcherDialog)
    }
    Shortcut {
        sequences: [StandardKey.ZoomIn, "Ctrl+Plus", "Ctrl+Equal", "Ctrl+Shift+Equal"]
        context: Qt.ApplicationShortcut

        onActivated: timelineRoot.adjustFontSize(1)
    }
    Shortcut {
        sequences: [StandardKey.ZoomOut, "Ctrl+Minus", "Ctrl+Underscore"]
        context: Qt.ApplicationShortcut

        onActivated: timelineRoot.adjustFontSize(-1)
    }
    Shortcut {
        // Add alternative shortcut, because sometimes Alt+A is stolen by the TextEdit
        sequences: ["Alt+A", "Ctrl+Shift+A"]

        onActivated: Rooms.nextRoomWithActivity()
    }
    Shortcut {
        sequences: ["Ctrl+Down", "Ctrl+PgDown"]

        onActivated: Rooms.nextRoom()
    }
    Shortcut {
        sequences: ["Ctrl+Up", "Ctrl+PgUp"]

        onActivated: Rooms.previousRoom()
    }
}
