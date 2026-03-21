// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property var appRoot
    readonly property string keyboardShortcutsGuideUrl: "https://github.com/etkecc/komai/blob/main/docs/user-guide/keyboard-shortcuts.md"
    readonly property int shortcutColumnWidth: 180
    readonly property var helpSections: [
        {
            "title": qsTr("Navigation"),
            "items": [
                {
                    "shortcut": qsTr("Up / k"),
                    "description": qsTr("Move toward older messages")
                },
                {
                    "shortcut": qsTr("Down / j"),
                    "description": qsTr("Move toward newer messages")
                },
                {
                    "shortcut": qsTr("Ctrl+U"),
                    "description": qsTr("Move about half a screen up")
                },
                {
                    "shortcut": qsTr("Ctrl+D"),
                    "description": qsTr("Move about half a screen down")
                },
                {
                    "shortcut": qsTr("gg"),
                    "description": qsTr("Go to the oldest loaded message")
                },
                {
                    "shortcut": qsTr("Shift+G"),
                    "description": qsTr("Go to the latest message in view")
                }
            ]
        },
        {
            "title": qsTr("Selection"),
            "items": [
                {
                    "shortcut": qsTr("Space"),
                    "description": qsTr("Toggle explicit selection for the focused message")
                }
            ]
        },
        {
            "title": qsTr("Actions"),
            "items": [
                {
                    "shortcut": qsTr("Enter"),
                    "description": qsTr("Open inline actions for the selected or focused message")
                },
                {
                    "shortcut": qsTr("r"),
                    "description": qsTr("Reply to the selected or focused message")
                },
                {
                    "shortcut": qsTr("t"),
                    "description": qsTr("Reply in thread or continue that thread")
                },
                {
                    "shortcut": qsTr("e"),
                    "description": qsTr("Edit the selected or focused message")
                },
                {
                    "shortcut": qsTr("f"),
                    "description": qsTr("Forward the selected or focused message")
                },
                {
                    "shortcut": qsTr("d"),
                    "description": qsTr("Delete message")
                },
                {
                    "shortcut": qsTr("u"),
                    "description": qsTr("View raw JSON for the selected or focused message")
                },
                {
                    "shortcut": qsTr("o"),
                    "description": qsTr("Open full Message actions for the selected or focused message")
                }
            ]
        },
        {
            "title": qsTr("Mode"),
            "items": [
                {
                    "shortcut": qsTr("?"),
                    "description": qsTr("Open this help")
                },
                {
                    "shortcut": qsTr("i"),
                    "description": qsTr("Exit selection mode and return to the composer")
                },
                {
                    "shortcut": qsTr("Escape"),
                    "description": qsTr("Close inline actions, clear selection, or exit selection mode")
                }
            ]
        }
    ]

    overlayViewport: appRoot
    overlayDialogMinWidth: 1040
    overlayDialogMaxWidthRatio: 0.98
    title: qsTr("Keyboard Shortcuts in Selection mode")
    titleIcon: ":/icons/icons/ui/keyboard-shortcut.svg"

    component ShortcutRow: Rectangle {
        id: shortcutRow

        required property string shortcutText
        required property string descriptionText

        Layout.fillWidth: true
        implicitHeight: rowLayout.implicitHeight + Komai.paddingSmall * 2
        radius: 6
        color: palette.base
        border.color: palette.mid
        border.width: 1

        RowLayout {
            id: rowLayout

            anchors.fill: parent
            anchors.margins: Komai.paddingSmall
            spacing: Komai.paddingMedium

            Label {
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: root.shortcutColumnWidth
                Layout.maximumWidth: root.shortcutColumnWidth
                color: palette.text
                font.bold: true
                text: shortcutRow.shortcutText
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                color: palette.text
                text: shortcutRow.descriptionText
                wrapMode: Text.WordWrap
            }
        }
    }

    Item {
        id: scrollArea

        readonly property bool showScrollbar: {
            switch (Settings.uiScrollbarPolicy) {
            case Settings.ScrollbarPolicy.Always:
                return true;
            case Settings.ScrollbarPolicy.Never:
                return false;
            case Settings.ScrollbarPolicy.WhenNeeded:
            default:
                return contentColumn.implicitHeight > helpFlickable.height;
            }
        }
        readonly property real reservedScrollbarWidth: showScrollbar
            ? Math.max(verticalScrollbar.width, verticalScrollbar.implicitWidth) + Komai.paddingSmall
            : 0

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: Math.min(contentColumn.implicitHeight, root.overlayDialogViewport ? root.overlayDialogViewport.height * 0.65 : 520)
        clip: true

        Flickable {
            id: helpFlickable

            anchors.fill: parent
            anchors.rightMargin: scrollArea.reservedScrollbarWidth
            contentWidth: width
            contentHeight: contentColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            ColumnLayout {
                id: contentColumn

                width: helpFlickable.width
                spacing: Komai.paddingLarge

                Text {
                    Layout.fillWidth: true
                    color: palette.buttonText
                    textFormat: Text.RichText
                    text: "<style>a { color: " + palette.highlight + "; }</style>"
                        + qsTr("These shortcuts apply in Selection mode after you close this help. See the <a href=\"%1\">full guide</a>.").arg(root.keyboardShortcutsGuideUrl)
                    wrapMode: Text.WordWrap

                    onLinkActivated: function(link) {
                        Qt.openUrlExternally(link);
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }

                Repeater {
                    model: root.helpSections

                    delegate: ColumnLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall

                        Label {
                            Layout.fillWidth: true
                            color: palette.text
                            font.bold: true
                            text: parent.modelData.title
                        }

                        Repeater {
                            model: parent.modelData.items

                            delegate: ShortcutRow {
                                required property var modelData

                                shortcutText: modelData.shortcut
                                descriptionText: modelData.description
                            }
                        }
                    }
                }
            }
        }

        ScrollBar {
            id: verticalScrollbar

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            orientation: Qt.Vertical
            z: 1
            policy: ScrollBar.AlwaysOn
            visible: scrollArea.showScrollbar
            opacity: scrollArea.showScrollbar ? 1 : 0

            Binding on position {
                when: !verticalScrollbar.pressed
                value: helpFlickable.contentHeight > 0 ? helpFlickable.visibleArea.yPosition : 0
            }

            size: helpFlickable.contentHeight > 0 ? helpFlickable.visibleArea.heightRatio : 1

            onPositionChanged: {
                if (pressed)
                    helpFlickable.contentY = position * Math.max(0, helpFlickable.contentHeight - helpFlickable.height);
            }
        }
    }
}
