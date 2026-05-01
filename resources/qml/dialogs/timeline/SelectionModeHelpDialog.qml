// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property var appRoot
    readonly property string keyboardShortcutsGuideUrl: "https://github.com/etkecc/komai/blob/main/docs/user-guide/features/keyboard-shortcuts.md"
    property string activeWarningKey: ""
    readonly property string selectionModeShortcutWarningText: qsTr("This keyboard shortcut only works in Selection mode, after closing Help.")
    readonly property string defaultIntroText: qsTr("These shortcuts apply in Selection mode after closing Help.")
        + ' <a href="' + root.keyboardShortcutsGuideUrl + '">'
        + qsTr("See the full guide")
        + "</a>."
    readonly property var helpSections: [
        {
            "title": qsTr("Movement"),
            "items": [
                {
                    "warningKey": "move-up",
                    "shortcuts": ["↑", qsTr("K")],
                    "label": qsTr("Move to older messages"),
                    "icon": ":/icons/icons/ui/chevron-up.svg"
                },
                {
                    "warningKey": "move-down",
                    "shortcuts": ["↓", qsTr("J")],
                    "label": qsTr("Move to newer messages"),
                    "icon": ":/icons/icons/ui/chevron-down.svg"
                },
                {
                    "warningKey": "page-up",
                    "shortcuts": [qsTr("Ctrl+U")],
                    "label": qsTr("Move about half a screen up"),
                    "icon": ":/icons/icons/ui/chevron-double-up.svg"
                },
                {
                    "warningKey": "page-down",
                    "shortcuts": [qsTr("Ctrl+D")],
                    "label": qsTr("Move about half a screen down"),
                    "icon": ":/icons/icons/ui/chevron-double-down.svg"
                },
                {
                    "warningKey": "jump-top-loaded",
                    "shortcuts": [qsTr("gg")],
                    "label": qsTr("Go to the oldest loaded message"),
                    "icon": ":/icons/icons/ui/upload.svg"
                },
                {
                    "warningKey": "jump-bottom-loaded",
                    "shortcuts": [qsTr("Shift+G")],
                    "label": qsTr("Go to the newest loaded message"),
                    "icon": ":/icons/icons/ui/download.svg"
                }
            ]
        },
        {
            "title": qsTr("Selection"),
            "items": [
                {
                    "warningKey": "toggle-selection",
                    //: Keyboard-key label (the spacebar). Use whatever is printed on the key in the target locale; do not translate as "outer space" / "area".
                    "shortcuts": [qsTr("Space")],
                    "label": qsTr("Toggle selection for the focused message"),
                    "icon": ":/icons/icons/ui/select-all-on.svg"
                }
            ]
        },
        {
            "title": qsTr("Actions"),
            "items": [
                {
                    "warningKey": "open-actions",
                    //: Keyboard-key label (Enter / Return key).
                    "shortcuts": [qsTr("Enter")],
                    "label": qsTr("Open inline actions for the selected or focused message"),
                    "icon": ":/icons/icons/ui/textbox-more.svg"
                },
                {
                    "warningKey": "copy-original-body",
                    "shortcuts": [qsTr("Ctrl+C")],
                    "label": qsTr("Copy original body for selected messages, or the selected or focused message"),
                    "icon": ":/icons/icons/ui/copy.svg"
                },
                {
                    "warningKey": "copy-plain-text",
                    "shortcuts": [qsTr("Ctrl+Shift+C")],
                    "label": qsTr("Copy plain text for selected messages, or the selected or focused message"),
                    "icon": ":/icons/icons/ui/copy.svg"
                },
                {
                    "warningKey": "reply",
                    "shortcuts": [qsTr("R")],
                    "label": qsTr("Reply to the selected or focused message"),
                    "icon": ":/icons/icons/ui/reply.svg"
                },
                {
                    "warningKey": "thread",
                    "shortcuts": [qsTr("T")],
                    "label": qsTr("Open or continue the selected or focused thread"),
                    "icon": ":/icons/icons/ui/thread.svg"
                },
                {
                    "warningKey": "edit",
                    "shortcuts": [qsTr("E")],
                    "label": qsTr("Edit the selected or focused message"),
                    "icon": ":/icons/icons/ui/edit.svg"
                },
                {
                    "warningKey": "forward",
                    "shortcuts": [qsTr("F")],
                    "label": qsTr("Forward selected messages, or the selected or focused message"),
                    "icon": ":/icons/icons/ui/forward.svg"
                },
                {
                    "warningKey": "delete",
                    "shortcuts": [qsTr("D")],
                    "label": qsTr("Delete selected messages, or the selected or focused message"),
                    "icon": ":/icons/icons/ui/delete.svg"
                },
                {
                    "warningKey": "view-raw",
                    "shortcuts": [qsTr("U")],
                    "label": qsTr("View raw JSON for the selected or focused message"),
                    "icon": ":/icons/icons/ui/raw-message.svg"
                },
                {
                    "warningKey": "open-message-actions",
                    "shortcuts": [qsTr("O")],
                    "label": qsTr("Open full Message actions for the selected or focused message"),
                    "icon": ":/icons/icons/ui/options-circle.svg"
                }
            ]
        },
        {
            "title": qsTr("Mode"),
            "items": [
                {
                    "warningKey": "open-help",
                    "shortcuts": [qsTr("?")],
                    "label": qsTr("Open this help"),
                    "icon": ":/icons/icons/ui/keyboard-shortcut.svg"
                },
                {
                    "warningKey": "return-to-composer",
                    "shortcuts": [qsTr("I")],
                    "label": qsTr("Exit Selection mode and return to the composer"),
                    "icon": ":/icons/icons/ui/send.svg"
                },
                {
                    "warningKey": "escape",
                    //: Keyboard-key label (Escape / Esc key).
                    "shortcuts": [qsTr("Escape")],
                    "label": qsTr("Close actions, clear selection, or exit Selection mode"),
                    "icon": ":/icons/icons/ui/dismiss.svg"
                }
            ]
        }
    ]

    overlayViewport: appRoot
    overlayDialogMinWidth: 1040
    overlayDialogMaxWidthRatio: 0.98
    title: qsTr("Keyboard Shortcuts in Selection mode")
    titleIcon: ":/icons/icons/ui/keyboard-shortcut.svg"
    readonly property var warningShortcutBindings: [
        { "sequence": "Up", "warningKey": "move-up" },
        { "sequence": "K", "warningKey": "move-up" },
        { "sequence": "Down", "warningKey": "move-down" },
        { "sequence": "J", "warningKey": "move-down" },
        { "sequence": "Ctrl+U", "warningKey": "page-up" },
        { "sequence": "Ctrl+D", "warningKey": "page-down" },
        { "sequence": "G, G", "warningKey": "jump-top-loaded" },
        { "sequence": "Shift+G", "warningKey": "jump-bottom-loaded" },
        { "sequence": "Space", "warningKey": "toggle-selection" },
        { "sequence": "Return", "warningKey": "open-actions" },
        { "sequence": "Enter", "warningKey": "open-actions" },
        { "sequence": "Ctrl+C", "warningKey": "copy-original-body" },
        { "sequence": "Ctrl+Shift+C", "warningKey": "copy-plain-text" },
        { "sequence": "R", "warningKey": "reply" },
        { "sequence": "T", "warningKey": "thread" },
        { "sequence": "E", "warningKey": "edit" },
        { "sequence": "F", "warningKey": "forward" },
        { "sequence": "D", "warningKey": "delete" },
        { "sequence": "U", "warningKey": "view-raw" },
        { "sequence": "O", "warningKey": "open-message-actions" },
        { "sequence": "?", "warningKey": "open-help" },
        { "sequence": "I", "warningKey": "return-to-composer" }
    ]

    function showSelectionModeShortcutWarning(warningKey) {
        activeWarningKey = warningKey;
        shortcutWarningResetTimer.restart();
    }

    Timer {
        id: shortcutWarningResetTimer

        interval: 1800
        repeat: false
        onTriggered: root.activeWarningKey = ""
    }

    Instantiator {
        model: root.warningShortcutBindings

        delegate: Shortcut {
            required property var modelData

            enabled: root.visible
            sequence: modelData.sequence
            context: Qt.ApplicationShortcut

            onActivated: root.showSelectionModeShortcutWarning(modelData.warningKey)
        }
    }

    component ShortcutBadgeGroup: Item {
        id: badgeGroup

        required property var shortcutTexts
        property string separatorText: "/"

        implicitWidth: badgeRow.implicitWidth
        implicitHeight: badgeRow.implicitHeight

        RowLayout {
            id: badgeRow

            anchors.fill: parent
            spacing: Komai.paddingSmall

            Repeater {
                model: badgeGroup.shortcutTexts

                delegate: RowLayout {
                    required property string modelData
                    required property int index

                    spacing: Komai.paddingSmall

                    Components.ShortcutKeyBadge {
                        Layout.alignment: Qt.AlignVCenter
                        text: modelData
                        showKeyboardIcon: index === 0
                        keyTextColor: palette.buttonText
                        toolTipText: ""
                    }

                    Label {
                        Layout.alignment: Qt.AlignVCenter
                        color: palette.buttonText
                        text: badgeGroup.separatorText
                        visible: index < badgeGroup.shortcutTexts.length - 1
                    }
                }
            }
        }
    }

    component ShortcutRow: Item {
        id: shortcutRow

        required property var shortcutTexts
        required property string labelText
        required property string warningKey
        property string shortcutSeparatorText: "/"
        property string iconSource: ""
        property bool mirrorIcon: false
        readonly property int actionIconSize: 24
        readonly property int controlHeight: Math.max(40, Math.round(Settings.uiFontSizePt * 2.9))
        readonly property bool showingWarning: root.activeWarningKey === warningKey

        function tintedIconSource(source)
        {
            if (!source)
                return "";

            let resolved = (typeof source.toString === "function") ? source.toString() : String(source);
            if (!resolved || resolved.length === 0)
                return "";
            if (resolved.startsWith("image://"))
                return resolved;
            if (resolved.startsWith("qrc:/"))
                resolved = ":" + resolved.substring(4);
            return "image://colorimage/" + resolved + "?" + palette.text;
        }

        Layout.fillWidth: true
        implicitHeight: Math.max(controlHeight,
                                 rowLayout.implicitHeight + Komai.paddingSmall * 2 + 4)

        Rectangle {
            anchors.fill: parent
            color: palette.window
            radius: Komai.paddingSmall
            border.color: Komai.theme.separator
            border.width: 1
        }

        RowLayout {
            id: rowLayout

            anchors.fill: parent
            anchors.leftMargin: Komai.paddingMedium + 2
            anchors.rightMargin: Komai.paddingMedium + 2
            anchors.topMargin: Komai.paddingSmall + 2
            anchors.bottomMargin: Komai.paddingSmall + 2
            spacing: Komai.paddingSmall

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: shortcutRow.actionIconSize
                Layout.preferredHeight: shortcutRow.actionIconSize
                fillMode: Image.PreserveAspectFit
                source: shortcutRow.iconSource.length > 0 ? shortcutRow.tintedIconSource(shortcutRow.iconSource) : ""
                sourceSize.width: shortcutRow.actionIconSize
                sourceSize.height: shortcutRow.actionIconSize
                visible: shortcutRow.iconSource.length > 0
                smooth: true

                transform: Scale {
                    origin.x: shortcutRow.actionIconSize / 2
                    xScale: shortcutRow.mirrorIcon ? -1 : 1
                }
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                color: shortcutRow.showingWarning ? Komai.theme.warning : palette.text
                font.bold: true
                text: shortcutRow.showingWarning ? root.selectionModeShortcutWarningText : shortcutRow.labelText
                wrapMode: Text.WordWrap
            }

            ShortcutBadgeGroup {
                Layout.alignment: Qt.AlignVCenter
                shortcutTexts: shortcutRow.shortcutTexts
                separatorText: shortcutRow.shortcutSeparatorText
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
                    text: "<style>a { color: " + palette.highlight + "; }</style>" + root.defaultIntroText
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

                        Components.SettingsSection {
                            Layout.fillWidth: true
                            label: parent.modelData.title
                        }

                        Repeater {
                            model: parent.modelData.items

                            delegate: ShortcutRow {
                                required property var modelData

                                shortcutTexts: modelData.shortcuts ?? []
                                labelText: modelData.label
                                warningKey: modelData.warningKey ?? ""
                                shortcutSeparatorText: modelData.shortcutSeparator ?? "/"
                                iconSource: modelData.icon ?? ""
                                mirrorIcon: !!modelData.mirrorIcon
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
