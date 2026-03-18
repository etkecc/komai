// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../ui"
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

RowLayout {
    id: root

    required property var room
    required property bool filteringInProgress
    required property int topBarAvatarSize
    property bool searchActive: false
    readonly property bool layoutVisible: searchActive
    readonly property bool searchHasFocus: searchField.focus && searchField.enabled

    signal searchStringCommitted(string value)
    signal requestClose()

    function focusInput() {
        searchField.forceActiveFocus();
    }

    function clearInput() {
        searchField.clear();
    }

    Layout.minimumHeight: 0
    Layout.preferredHeight: layoutVisible ? implicitHeight : 0
    Layout.maximumHeight: layoutVisible ? implicitHeight : 0
    Layout.topMargin: Komai.paddingSmall
    spacing: Komai.paddingSmall
    visible: layoutVisible

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
        radius: Komai.paddingSmall

        onEditingFinished: root.searchStringCommitted(text)
    }
    ImageButton {
        Layout.preferredHeight: root.topBarAvatarSize
        Layout.preferredWidth: root.topBarAvatarSize
        toolTipText: qsTr("Close search")
        toolTipVisible: hovered
        image: ":/icons/icons/ui/dismiss.svg"

        onClicked: root.requestClose()
    }
}
