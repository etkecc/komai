// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    property string roomId: ""
    property bool autoSave: false
    property bool togglesEnabled: Settings.hasActiveSession

    implicitWidth: 680
    implicitHeight: content.implicitHeight
    width: parent ? parent.width : implicitWidth

    readonly property bool useStackedLayout: width < 640

    function normalizeEventTypes(eventTypes) {
        return Array.isArray(eventTypes) ? eventTypes : [eventTypes];
    }

    function isEventShown(eventType) {
        return !hiddenEvents.hiddenEvents.includes(eventType);
    }

    function areEventsShown(eventTypes) {
        const normalized = normalizeEventTypes(eventTypes);
        for (let i = 0; i < normalized.length; ++i) {
            if (!isEventShown(normalized[i]))
                return false;
        }

        return true;
    }

    function setEventsShown(eventTypes, shouldShow) {
        const normalized = normalizeEventTypes(eventTypes);
        let changed = false;

        for (let i = 0; i < normalized.length; ++i) {
            const eventType = normalized[i];
            if (isEventShown(eventType) === shouldShow)
                continue;

            hiddenEvents.toggle(eventType);
            changed = true;
        }

        if (changed && autoSave)
            hiddenEvents.save();
    }

    function save() {
        hiddenEvents.save();
    }

    function reloadHiddenEvents() {
        hiddenEvents.roomid = root.roomId;
    }

    HiddenEvents {
        id: hiddenEvents

        roomid: root.roomId
    }

    component HiddenEventToggleRow: Item {
        id: rowRoot

        required property string label
        required property string description
        required property var eventTypes

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

                RowLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: Komai.paddingSmall

                    Text {
                        Layout.minimumWidth: 0
                        color: rowHover.hovered ? palette.brightText : palette.text
                        text: rowRoot.label
                        font.pointSize: 1.1 * Settings.uiFontSizePt
                        wrapMode: Text.Wrap
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                SettingControlToggle {
                    id: toggle

                    Layout.row: root.useStackedLayout ? 1 : 0
                    Layout.column: root.useStackedLayout ? 0 : 1
                    Layout.alignment: (root.useStackedLayout ? Qt.AlignLeft : Qt.AlignRight)
                        | Qt.AlignTop
                    value: root.areEventsShown(rowRoot.eventTypes)
                    textColor: rowHover.hovered ? palette.brightText : palette.buttonText
                    enabled: root.togglesEnabled

                    onToggledValue: function(value) {
                        root.setEventsShown(rowRoot.eventTypes, value);
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingSmall
                Layout.rightMargin: Komai.paddingSmall
                Layout.topMargin: -Komai.paddingSmall
                color: rowHover.hovered ? palette.brightText : palette.buttonText
                text: rowRoot.description
                font.pointSize: Settings.uiFontSizePt
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

        anchors.fill: parent
        spacing: Komai.paddingSmall

        HiddenEventToggleRow {
            label: qsTr("Show member changes")
            description: qsTr("Joins, leaves, bans, display name changes, and avatar changes.")
            eventTypes: [MtxEvent.Member]
        }

        HiddenEventToggleRow {
            label: qsTr("Show power level changes")
            description: qsTr("Moderator changes and room permission updates.")
            eventTypes: [MtxEvent.PowerLevels]
        }

        HiddenEventToggleRow {
            label: qsTr("Show stickers")
            description: qsTr("Show sticker events in the timeline.")
            eventTypes: [MtxEvent.Sticker]
        }

        HiddenEventToggleRow {
            label: qsTr("Show reactions as events")
            description: qsTr("Separate reaction events, not just reaction pills.")
            eventTypes: [MtxEvent.Reaction]
        }

        HiddenEventToggleRow {
            label: qsTr("Show server access changes")
            description: qsTr("Allowed and blocked homeserver list changes.")
            eventTypes: [MtxEvent.ServerAcl]
        }
    }
}
