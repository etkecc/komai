// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    property var profile: Komai.currentUser
    property string currentUserId: profile && profile.userid ? profile.userid : ""
    property string lastSentStatusMessage: ""

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    function refreshFromPresence()
    {
        if (statusMessageField.activeFocus)
            return;

        statusMessageField.text = Komai.statusMessage();
        lastSentStatusMessage = statusMessageField.text;
    }

    function applyStatusMessage()
    {
        const nextValue = statusMessageField.text;
        if (nextValue === lastSentStatusMessage)
            return;

        lastSentStatusMessage = nextValue;
        Komai.setStatusMessage(nextValue);
    }

    RowLayout {
        id: row

        anchors.fill: parent
        spacing: Komai.paddingSmall

        TextField {
            id: statusMessageField

            Layout.fillWidth: true
            font.pointSize: Settings.uiFontSizePt
            selectByMouse: true
            placeholderText: qsTr("Set a status message")
            onAccepted: {
                root.applyStatusMessage();
            }
            onEditingFinished: {
                root.applyStatusMessage();
            }
            onActiveFocusChanged: if (!activeFocus)
                {
                    root.applyStatusMessage();
                }
        }

        ImageButton {
            id: clearStatusButton

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            image: ":/icons/icons/ui/round-remove-button.svg"
            visible: statusMessageField.text !== ""
            ToolTip.visible: hovered
            ToolTip.delay: Komai.tooltipDelay
            ToolTip.text: qsTr("Clear status message")

            onClicked: {
                statusMessageField.text = "";
                root.applyStatusMessage();
            }
        }
    }

    Connections {
        target: Presence

        function onPresenceChanged(id)
        {
            if (id === root.currentUserId)
                root.refreshFromPresence();
        }
    }

    Connections {
        target: Komai

        function onProfileChanged()
        {
            root.profile = Komai.currentUser;
            root.refreshFromPresence();
        }
    }

    onVisibleChanged: if (!visible) {
        root.applyStatusMessage();
    }

    Component.onCompleted: refreshFromPresence()
    Component.onDestruction: {
        root.applyStatusMessage();
    }
}
