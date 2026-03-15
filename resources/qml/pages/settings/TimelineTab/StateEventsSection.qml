// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../../../components" as Components
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    implicitHeight: content.implicitHeight
    width: parent ? parent.width : 0

    readonly property bool useStackedLayout: width < 700
    readonly property real controlWidth: useStackedLayout
        ? Math.max(0, width - Komai.paddingSmall * 2)
        : Math.min(500, Math.max(240, width - Komai.paddingLarge * 2))

    function isEventShown(eventType) {
        return !hiddenEvents.hiddenEvents.includes(eventType);
    }

    function setEventShown(eventType, shouldShow) {
        if (root.isEventShown(eventType) === shouldShow) {
            return;
        }

        hiddenEvents.toggle(eventType);
        hiddenEvents.save();
    }

    function reloadHiddenEvents() {
        hiddenEvents.roomid = "";
    }

    HiddenEvents {
        id: hiddenEvents

        roomid: ""
    }

    component ExtraEventToggleRow: Item {
        id: rowRoot

        required property string label
        required property string description
        required property int eventType

        Layout.fillWidth: true
        implicitHeight: rowContent.implicitHeight

        HoverHandler {
            id: rowHover

            blocking: false
            enabled: toggle.enabled
        }

        Rectangle {
            anchors.fill: rowContent
            color: rowHover.hovered ? palette.dark : palette.window
            radius: Komai.paddingMedium
            z: -1
        }

        ColumnLayout {
            id: rowContent

            width: parent.width
            spacing: Komai.paddingMedium

            GridLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingSmall
                Layout.rightMargin: Komai.paddingSmall
                columns: root.useStackedLayout ? 1 : 2
                rowSpacing: root.useStackedLayout ? Komai.paddingSmall : 0
                columnSpacing: Komai.paddingSmall

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    color: rowHover.hovered ? palette.brightText : palette.text
                    text: rowRoot.label
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    wrapMode: Text.Wrap
                }

                Components.SettingControlToggle {
                    id: toggle

                    Layout.row: root.useStackedLayout ? 1 : 0
                    Layout.column: root.useStackedLayout ? 0 : 1
                    Layout.alignment: (root.useStackedLayout ? Qt.AlignLeft : Qt.AlignRight) | Qt.AlignTop
                    value: root.isEventShown(rowRoot.eventType)
                    enabled: Settings.hasActiveSession
                    onToggledValue: function(value) {
                        root.setEventShown(rowRoot.eventType, value);
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingSmall
                Layout.rightMargin: Komai.paddingSmall
                Layout.topMargin: -Komai.paddingSmall
                Layout.bottomMargin: Komai.paddingMedium
                color: rowHover.hovered ? palette.brightText : palette.buttonText
                text: rowRoot.description
                font.pointSize: 0.9 * Settings.uiFontSizePt
                wrapMode: Text.Wrap
            }
        }
    }

    Connections {
        target: Settings

        function onProfileChanged() {
            root.reloadHiddenEvents();
        }

        function onUserIdChanged() {
            root.reloadHiddenEvents();
        }

        function onSessionAuthStateChanged() {
            root.reloadHiddenEvents();
        }
    }

    ColumnLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Komai.paddingSmall

        Components.SettingsSection {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingLarge
            Layout.bottomMargin: Komai.paddingSmall
            label: qsTr("Additional events")
            helperText: Settings.hasActiveSession
                ? qsTr("If you're feeling overwhelmed, consider disabling some of these noisy events.")
                : qsTr("Available after you sign in.")
        }

        ExtraEventToggleRow {
            label: qsTr("Show member changes")
            description: qsTr("Joins, leaves, bans, display-name changes, and avatar changes.")
            eventType: MtxEvent.Member
        }

        ExtraEventToggleRow {
            label: qsTr("Show power level changes")
            description: qsTr("Moderator changes and permission updates.")
            eventType: MtxEvent.PowerLevels
        }

        ExtraEventToggleRow {
            label: qsTr("Show stickers")
            description: qsTr("Display sticker events in the timeline.")
            eventType: MtxEvent.Sticker
        }

        ExtraEventToggleRow {
            label: qsTr("Show server access changes")
            description: qsTr("Changes to the list of allowed or blocked homeservers.")
            eventType: MtxEvent.ServerAcl
        }
    }
}
