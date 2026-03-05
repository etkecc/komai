// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../ui"
import "../../../components"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Item {
    id: membersTab

    property var roomSettings
    property var members
    property var room
    property var appRoot

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

        KomaiButton {
            Layout.alignment: Qt.AlignHCenter
            icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
            text: qsTr("Invite others")
            onClicked: {
                if (membersTab.members)
                    TimelineManager.openInviteUsers(membersTab.members.roomId);
            }
        }

        MatrixTextField {
            id: searchBar

            Layout.fillWidth: true
            placeholderText: qsTr("Search...")
            onTextChanged: {
                if (membersTab.members)
                    membersTab.members.setFilterString(text);
            }
        }

        RowLayout {
            spacing: Komai.paddingMedium

            Label {
                text: qsTr("Sort by: ")
                color: palette.text
            }

            KomaiComboBox {
                model: ListModel {
                    ListElement { data: MemberList.Mxid; text: qsTr("User ID") }
                    ListElement { data: MemberList.DisplayName; text: qsTr("Display name") }
                    ListElement { data: MemberList.Powerlevel; text: qsTr("Power level") }
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

        ScrollView {
            padding: Komai.paddingMedium
            ScrollBar.horizontal.visible: false
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: memberList

                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: membersTab.members

                delegate: ItemDelegate {
                    id: del

                    property bool isCurrentUser: {
                        const currentUser = Komai.currentUser;
                        const currentUserId = (currentUser && currentUser.userid)
                                ? String(currentUser.userid)
                                : "";
                        return currentUserId.length > 0 && model && model.mxid === currentUserId;
                    }

                    onClicked: {
                        if (membersTab.room)
                            membersTab.room.openUserProfile(model.mxid);
                    }
                    padding: Komai.paddingMedium
                    width: ListView.view.width
                    height: memberLayout.implicitHeight + Komai.paddingSmall * 2
                    hoverEnabled: true
                    background: Rectangle {
                        color: del.hovered ? palette.dark : palette.window
                    }

                    RowLayout {
                        id: memberLayout

                        spacing: Komai.paddingMedium
                        anchors.centerIn: parent
                        width: parent.width - Komai.paddingSmall * 2

                        Avatar {
                            id: avatar

                            Layout.preferredWidth: Komai.avatarSize
                            Layout.preferredHeight: Komai.avatarSize
                            userid: model.mxid
                            url: model.avatarUrl.replace("mxc://", "image://MxcImage/")
                            displayName: model.displayName
                            enabled: false
                        }

                        ColumnLayout {
                            spacing: Komai.paddingSmall
                            Layout.fillWidth: true

                            ElidedLabel {
                                fullText: model.displayName
                                color: del.isCurrentUser
                                    ? palette.highlight
                                    : Qt.darker(membersTab.room ? TimelineManager.roomUserColor(membersTab.room.roomId, model ? model.mxid : "", del.background.color, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(model ? model.mxid : "", del.background.color), 1.3)
                                font.pixelSize: fontMetrics.font.pixelSize
                                elideWidth: del.width - Komai.paddingMedium * 2 - avatar.width - encryptInd.width
                                Layout.fillWidth: true
                            }

                            ElidedLabel {
                                fullText: model.mxid
                                color: del.hovered ? palette.brightText : palette.buttonText
                                font.pixelSize: Math.ceil(fontMetrics.font.pixelSize * 0.9)
                                elideWidth: del.width - Komai.paddingMedium * 2 - avatar.width - encryptInd.width
                                Layout.fillWidth: true
                            }
                        }

                        PowerlevelIndicator {
                            Layout.preferredWidth: fontMetrics.lineSpacing * 2
                            Layout.preferredHeight: fontMetrics.lineSpacing * 2
                            sourceSize.width: width
                            sourceSize.height: height
                            powerlevel: model.powerlevel
                            permissions: membersTab.room ? membersTab.room.permissions : null
                        }

                        EncryptionIndicator {
                            id: encryptInd

                            Layout.preferredWidth: fontMetrics.lineSpacing * 2
                            Layout.preferredHeight: fontMetrics.lineSpacing * 2
                            sourceSize.width: width
                            sourceSize.height: height
                            Layout.alignment: Qt.AlignRight
                            visible: membersTab.room ? membersTab.room.isEncrypted : false
                            encrypted: membersTab.room ? membersTab.room.isEncrypted : false
                            trust: encrypted ? model.trustlevel : Crypto.Unverified
                            ToolTip.text: {
                                if (!encrypted)
                                    return qsTr("This room is not encrypted!");

                                switch (trust) {
                                case Crypto.Verified:
                                    return qsTr("This user is verified.");
                                case Crypto.TOFU:
                                    return qsTr("This user isn't verified, but is still using the same master key from the first time you met.");
                                default:
                                    return qsTr("This user has unverified devices!");
                                }
                            }
                        }
                    }

                    KomaiCursorShape {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
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
