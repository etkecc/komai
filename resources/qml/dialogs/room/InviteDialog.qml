// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

OverlayDialog {
    id: inviteDialogRoot

    property InviteesModel invitees
    property var friendsCompleter
    property var profile

    title: qsTr("Invite users to %1").arg(invitees.room.plainRoomName)
    titleIcon: ":/icons/icons/ui/people.svg"
    initialFocusItem: inviteeEntry

    Component.onCompleted: {
        friendsCompleter = TimelineManager.completerFor("user", "friends");
    }

    function addInvite(mxid, displayName, avatarUrl) {
        if (mxid.match("@.+?:.{3,}"))
            invitees.addUser(mxid, displayName, avatarUrl);
        else
            console.log("invalid mxid: " + mxid);
    }

    function cleanUpAndClose() {
        if (inviteeEntry.isValidMxid)
            addInvite(inviteeEntry.text, "", "");
        invitees.accept();
        close();
    }

    Shortcut {
        sequence: "Ctrl+Enter"
        onActivated: inviteDialogRoot.cleanUpAndClose()
    }

    Flow {
        layoutDirection: Qt.LeftToRight
        Layout.fillWidth: true
        Layout.preferredHeight: implicitHeight
        spacing: 4
        visible: !inviteesList.visible

        Repeater {
            model: inviteDialogRoot.invitees

            delegate: ItemDelegate {
                id: inviteeButton

                onClicked: inviteDialogRoot.invitees.removeUser(model.mxid)

                contentItem: Label {
                    anchors.centerIn: parent
                    text: model.displayName != "" ? model.displayName : model.userid
                    color: inviteeButton.hovered ? palette.highlightedText : palette.text
                    maximumLineCount: 1
                }

                background: Rectangle {
                    border.color: palette.text
                    color: inviteeButton.hovered ? palette.highlight : palette.window
                    border.width: 1
                    radius: inviteeButton.height / 2
                }
            }
        }
    }

    Label {
        text: qsTr("Search user")
        Layout.fillWidth: true
        color: palette.text
    }

    RowLayout {
        spacing: Komai.paddingMedium
        Layout.fillWidth: true

        MatrixTextField {
            id: inviteeEntry

            property bool isValidMxid: text.match("@.+?:.{3,}")

            backgroundColor: palette.window
            placeholderText: qsTr("@user:yourserver.example.com", "Example user id. The name 'user' can be localized however you want.")
            Layout.fillWidth: true
            onAccepted: {
                if (isValidMxid) {
                    inviteDialogRoot.addInvite(text, "", "");
                    clear();
                } else if (userSearch.count > 0) {
                    inviteDialogRoot.addInvite(userSearch.itemAtIndex(0).userid, userSearch.itemAtIndex(0).displayName, userSearch.itemAtIndex(0).avatarUrl);
                    clear();
                }
            }
            Keys.onShortcutOverride: event.accepted = ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier))
            Keys.onPressed: {
                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers === Qt.ControlModifier))
                    inviteDialogRoot.cleanUpAndClose();
            }
            onTextChanged: {
                searchTimer.restart();
                if (isValidMxid)
                    inviteDialogRoot.profile = TimelineManager.getGlobalUserProfile(text);
                else
                    inviteDialogRoot.profile = null;
            }

            Timer {
                id: searchTimer

                interval: 350
                onTriggered: {
                    userSearch.model.setSearchString(parent.text);
                }
            }
        }

        ToggleButton {
            id: searchOnServer

            checked: false
            onClicked: userSearch.model.setSearchString(inviteeEntry.text)
        }

        MatrixText {
            text: qsTr("Search on Server")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 250

        UserListRow {
            id: del3

            visible: inviteeEntry.isValidMxid
            Layout.preferredWidth: inviteDialogRoot.width / 2
            Layout.alignment: Qt.AlignTop
            Layout.preferredHeight: implicitHeight
            displayName: inviteDialogRoot.profile ? inviteDialogRoot.profile.displayName : ""
            avatarUrl: inviteDialogRoot.profile ? inviteDialogRoot.profile.avatarUrl : ""
            userid: inviteeEntry.text
            onClicked: inviteDialogRoot.addInvite(inviteeEntry.text, displayName, avatarUrl)
            bgColor: del3.hovered ? palette.dark : palette.window
        }

        ListView {
            id: userSearch

            visible: !inviteeEntry.isValidMxid
            model: searchOnServer.checked ? userDirectory : inviteDialogRoot.friendsCompleter
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            delegate: UserListRow {
                id: del2

                width: ListView.view.width
                height: implicitHeight
                displayName: model.displayName
                userid: model.userid
                avatarUrl: model.avatarUrl
                onClicked: inviteDialogRoot.addInvite(userid, displayName, avatarUrl)
                bgColor: del2.hovered ? palette.dark : palette.window
            }
        }

        Rectangle {
            Layout.fillHeight: true
            visible: inviteesList.visible
            Layout.preferredWidth: 1
            color: Komai.theme.separator
        }

        ListView {
            id: inviteesList

            Layout.fillWidth: true
            Layout.fillHeight: true
            model: inviteDialogRoot.invitees
            clip: true
            visible: inviteDialogRoot.width >= 500

            delegate: UserListRow {
                id: del

                hoverEnabled: true
                width: ListView.view.width
                height: implicitHeight
                onClicked: TimelineManager.openGlobalUserProfile(model.mxid)
                userid: model.mxid
                avatarUrl: model.avatarUrl
                displayName: model.displayName
                bgColor: del.hovered ? palette.dark : palette.window

                ImageButton {
                    id: removeButton

                    anchors.right: parent.right
                    anchors.rightMargin: Komai.paddingSmall
                    anchors.top: parent.top
                    anchors.topMargin: Komai.paddingSmall
                    image: ":/icons/icons/ui/dismiss.svg"
                    onClicked: inviteDialogRoot.invitees.removeUser(model.mxid)
                }

                KomaiCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Invite")
        highlighted: true
        enabled: invitees.count > 0
        onClicked: inviteDialogRoot.cleanUpAndClose()
    }
}
