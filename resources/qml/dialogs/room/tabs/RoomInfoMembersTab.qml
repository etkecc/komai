// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../ui"
import "../../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    id: membersTab

    property var roomSettings
    property var members
    property var room
    property var appRoot

    MatrixRoomPermissions {
        id: memberPermissions
        roomId: membersTab.members ? membersTab.members.roomId : ""
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingMedium

        Avatar {
            id: roomAvatar

            Layout.preferredHeight: 80
            Layout.preferredWidth: 80
            roomid: membersTab.members ? membersTab.members.roomId : ""
            displayName: membersTab.members ? membersTab.members.roomName : ""
            Layout.alignment: Qt.AlignHCenter
            url: membersTab.members ? membersTab.members.avatarUrl.replace("mxc://", "image://MxcImage/") : ""
            enabled: false
        }

        ElidedLabel {
            font.pixelSize: fontMetrics.font.pixelSize * 1.5
            fullText: membersTab.members ? qsTr("%n member(s) in %1", "Summary above list of members", membersTab.members.memberCount).arg(membersTab.members.roomName) : ""
            Layout.alignment: Qt.AlignHCenter
            elideWidth: parent.width - Komai.paddingMedium
        }

        SettingsSection {
            label: qsTr("Actions")
            Layout.fillWidth: true
        }

        KomaiActionRowButton {
            id: myProfileBtn

            Layout.fillWidth: true
            labelText: qsTr("Manage my profile in this room")
            iconSource: ":/icons/icons/ui/person.svg"

            onClicked: {
                const currentUser = Komai.currentUser;
                if (currentUser && currentUser.userid)
                    TimelineManager.openRoomUserProfile(membersTab.members.roomId, currentUser.userid);
            }

        }

        KomaiActionRowButton {
            id: inviteBtn

            Layout.fillWidth: true
            labelText: qsTr("Invite others")
            iconSource: ":/icons/icons/ui/plus-circle.svg"

            onClicked: {
                if (membersTab.members)
                    TimelineManager.openInviteUsers(membersTab.members.roomId);
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Komai.paddingLarge

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                color: palette.buttonText
                height: 1
            }
        }

        Flow {
            id: filterFlow

            Layout.fillWidth: true
            spacing: Komai.paddingMedium

            KomaiTextField {
                id: searchBar

                width: filterFlow.width >= 500
                    ? filterFlow.width - sortRow.width - filterFlow.spacing
                    : filterFlow.width
                placeholderText: qsTr("Search...")
                onTextChanged: {
                    if (membersTab.members)
                        membersTab.members.setFilterString(text);
                }
            }

            RowLayout {
                id: sortRow

                width: filterFlow.width >= 500
                    ? implicitWidth
                    : filterFlow.width
                spacing: Komai.paddingMedium

                Label {
                    text: qsTr("Sort by: ")
                    color: palette.text
                }

                KomaiComboBox {
                    model: ListModel {
                        ListElement { data: MemberList.PowerlevelThenName; text: qsTr("Power level, then name") }
                        ListElement { data: MemberList.Powerlevel; text: qsTr("Power level") }
                        ListElement { data: MemberList.DisplayName; text: qsTr("Display name, alphabetical") }
                        ListElement { data: MemberList.Mxid; text: qsTr("User ID, alphabetical") }
                    }
                    textRole: "text"
                    valueRole: "data"
                    onCurrentValueChanged: {
                        if (membersTab.members)
                            membersTab.members.sortBy(currentValue);
                    }
                    Layout.fillWidth: true
                }
            }
        }

        ScrollView {
            padding: Komai.paddingMedium
            rightPadding: Komai.paddingMedium + Komai.paddingSmall
            ScrollBar.horizontal.visible: false
            ScrollBar.vertical.policy: Settings.uiScrollbarPolicy === Settings.ScrollbarPolicy.Always ? ScrollBar.AlwaysOn : Settings.uiScrollbarPolicy === Settings.ScrollbarPolicy.Never ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
            ScrollBar.vertical.contentItem: Rectangle {
                implicitWidth: 6
                radius: 3
                color: palette.dark
                opacity: memberList.contentHeight > memberList.height ? 1 : 0
            }
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: memberList

                clip: true
                boundsBehavior: Flickable.StopAtBounds
                spacing: Komai.paddingSmall
                model: membersTab.members

                delegate: Rectangle {
                    id: del

                    property bool isCurrentUser: {
                        const currentUser = Komai.currentUser;
                        const currentUserId = (currentUser && currentUser.userid)
                                ? String(currentUser.userid)
                                : "";
                        return currentUserId.length > 0 && model && model.mxid === currentUserId;
                    }

                    width: ListView.view.width
                    implicitHeight: memberLayout.implicitHeight + Komai.paddingSmall * 2
                    color: delHover.hovered ? palette.dark : palette.window
                    radius: Komai.paddingMedium
                    border.color: Komai.theme.separator
                    border.width: 1

                    HoverHandler {
                        id: delHover
                        blocking: false
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton
                        onClicked: {
                            TimelineManager.openRoomUserProfile(membersTab.members.roomId, model.mxid);
                        }
                    }

                    RowLayout {
                        id: memberLayout

                        anchors.fill: parent
                        anchors.margins: Komai.paddingSmall
                        spacing: Komai.paddingMedium

                        AvatarUserFlipButton {
                            Layout.preferredWidth: Komai.listIconSize
                            Layout.preferredHeight: Komai.listIconSize
                            Layout.alignment: Qt.AlignVCenter
                            avatarButtonSize: Komai.listIconSize
                            cleanFront: true
                            avatarDisplayName: model.displayName
                            avatarUrl: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                            avatarUserId: model.mxid
                            badgeIconSource: ":/icons/icons/ui/person.svg"
                            onLeftClicked: {
                                TimelineManager.openRoomUserProfile(membersTab.members.roomId, model.mxid);
                            }
                        }

                        ColumnLayout {
                            spacing: Komai.paddingSmall
                            Layout.fillWidth: true

                            RowLayout {
                                spacing: Komai.paddingSmall
                                Layout.fillWidth: true

                                ElidedLabel {
                                    fullText: model.displayName
                                    color: delHover.hovered
                                        ? palette.brightText
                                        : (del.isCurrentUser ? palette.highlight : palette.text)
                                    font.pixelSize: fontMetrics.font.pixelSize
                                    elideWidth: del.width - Komai.paddingMedium * 4 - Komai.listIconSize - plBadge.width - encryptInd.width
                                    Layout.fillWidth: true
                                }

                                Rectangle {
                                    id: plBadge

                                    Layout.alignment: Qt.AlignVCenter
                                    implicitWidth: plBadgeRow.implicitWidth + Komai.paddingSmall * 2
                                    implicitHeight: plBadgeRow.implicitHeight + Komai.paddingSmall
                                    radius: Komai.paddingSmall
                                    color: "transparent"
                                    visible: true

                                    RowLayout {
                                        id: plBadgeRow

                                        anchors.centerIn: parent
                                        spacing: 2

                                        PowerlevelIndicator {
                                            id: plIcon

                                            Layout.preferredWidth: Math.ceil(fontMetrics.font.pixelSize * 0.9)
                                            Layout.preferredHeight: Math.ceil(fontMetrics.font.pixelSize * 0.9)
                                            sourceSize.width: width
                                            sourceSize.height: height
                                            powerlevel: model.powerlevel
                                            isCreator: model.isCreator
                                            permissions: memberPermissions
                                            iconColor: delHover.hovered ? palette.brightText : palette.buttonText
                                        }

                                        Label {
                                            text: plIcon.roleName
                                            color: delHover.hovered ? palette.brightText : palette.buttonText
                                            font.pixelSize: Math.ceil(fontMetrics.font.pixelSize * 0.8)
                                        }
                                    }
                                }
                            }

                            ElidedLabel {
                                fullText: model.mxid
                                color: delHover.hovered ? palette.brightText : palette.buttonText
                                font.pixelSize: Math.ceil(fontMetrics.font.pixelSize * 0.9)
                                elideWidth: del.width - Komai.paddingMedium * 4 - Komai.listIconSize - encryptInd.width
                                Layout.fillWidth: true
                            }
                        }

                        EncryptionIndicator {
                            id: encryptInd

                            Layout.preferredWidth: fontMetrics.lineSpacing * 2
                            Layout.preferredHeight: fontMetrics.lineSpacing * 2
                            sourceSize.width: width
                            sourceSize.height: height
                            Layout.alignment: Qt.AlignRight
                            hovered: delHover.hovered
                            encryptedHoverEnabled: false
                            visible: membersTab.room ? membersTab.room.isEncrypted : false
                            encrypted: membersTab.room ? membersTab.room.isEncrypted : false
                            trust: encrypted ? model.trustlevel : Crypto.Unverified
                            toolTipText: ""
                        }
                    }

                }

                footer: Item {
                    width: parent.width
                    visible: membersTab.members && (membersTab.members.numUsersLoaded < membersTab.members.memberCount) && membersTab.members.loadingMoreMembers
                    height: membersLoadingSpinner.implicitHeight
                    anchors.margins: Komai.paddingMedium

                    Spinner {
                        id: membersLoadingSpinner

                        anchors.centerIn: parent
                        implicitHeight: parent.visible ? 35 : 0
                    }
                }
            }
        }
    }
}
