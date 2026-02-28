// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

RowLayout {
    id: root

    required property var room
    required property bool filteringInProgress
    required property int topBarAvatarSize
    property bool searchActive: false
    readonly property bool searchHasFocus: searchField.focus && searchField.enabled

    signal searchStringCommitted(string value)
    signal requestClose()

    function focusInput() {
        searchField.forceActiveFocus();
    }

    function clearInput() {
        searchField.clear();
    }

    Layout.column: 1
    Layout.columnSpan: 9
    Layout.fillWidth: true
    Layout.preferredHeight: visible ? implicitHeight : 0
    Layout.row: 5
    Layout.topMargin: Nheko.paddingSmall
    spacing: Nheko.paddingSmall
    visible: searchActive

    RoomSearchStatusIcon {
        room: root.room
        filteringInProgress: root.filteringInProgress
        topBarAvatarSize: root.topBarAvatarSize
    }
    MatrixTextField {
        id: searchField

        Layout.fillWidth: true
        enabled: root.searchActive
        hasClear: false
        placeholderText: qsTr("Type to search in this room's messages")
        radius: Nheko.paddingSmall

        onEditingFinished: root.searchStringCommitted(text)
    }
    ImageButton {
        Layout.preferredHeight: root.topBarAvatarSize
        Layout.preferredWidth: root.topBarAvatarSize
        ToolTip.text: qsTr("Close search")
        ToolTip.visible: hovered
        image: ":/icons/icons/ui/dismiss.svg"

        onClicked: root.requestClose()
    }
}
