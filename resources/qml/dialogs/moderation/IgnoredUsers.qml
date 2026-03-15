// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Components.OverlayDialog {
    id: ignoredUsers

    title: qsTr("Ignored users")
    titleIcon: ":/icons/icons/ui/ban.svg"

    width: Math.round((parent ? parent.width : 500) * 0.6)

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: palette.text
        text: qsTr("Ignoring a user hides their messages (they can still see yours!).")
    }

    ScrollView {
        id: userListScrollView

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(
            userListContent.implicitHeight,
            ignoredUsers.parent ? ignoredUsers.parent.height * 0.6 : 400)
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: userListContent

            width: userListScrollView.availableWidth
            spacing: Komai.paddingSmall

            Repeater {
                model: TimelineManager.ignoredUsers

                delegate: AbstractButton {
                    id: userRow

                    required property string modelData

                    property var profile: TimelineManager.getGlobalUserProfile(modelData)
                    readonly property bool activeState: hovered || pressed || activeFocus

                    Layout.fillWidth: true
                    implicitHeight: userRowContent.implicitHeight + topPadding + bottomPadding
                    leftPadding: Komai.paddingMedium
                    rightPadding: Komai.paddingMedium
                    topPadding: Komai.paddingSmall
                    bottomPadding: Komai.paddingSmall
                    hoverEnabled: true
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus

                    onClicked: TimelineManager.openGlobalUserProfile(modelData)

                    background: Rectangle {
                        radius: Komai.paddingMedium
                        color: userRow.activeState ? palette.dark : palette.window
                    }

                    Components.KomaiCursorShape {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                    }

                    contentItem: RowLayout {
                        id: userRowContent

                        spacing: Komai.paddingMedium

                        Components.AvatarUserFlipButton {
                            id: avatarFlip

                            Layout.preferredWidth: avatarSide
                            Layout.preferredHeight: avatarSide
                            Layout.alignment: Qt.AlignVCenter

                            property int avatarSide: Komai.avatarSize

                            avatarButtonSize: avatarSide
                            cleanFront: true
                            avatarDisplayName: userRow.profile ? userRow.profile.displayName : ""
                            avatarUrl: userRow.profile ? userRow.profile.avatarUrl.replace("mxc://", "image://MxcImage/") : ""
                            avatarUserId: userRow.modelData

                            onLeftClicked: TimelineManager.openGlobalUserProfile(userRow.modelData)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: userRow.profile ? userRow.profile.displayName : userRow.modelData
                                color: userRow.activeState ? palette.brightText : palette.text
                                elide: Text.ElideRight
                                font.pointSize: Settings.uiFontSizePt
                            }

                            Label {
                                Layout.fillWidth: true
                                text: userRow.modelData
                                color: userRow.activeState ? palette.brightText : palette.buttonText
                                elide: Text.ElideRight
                                font.pointSize: Math.round(Settings.uiFontSizePt * 0.9)
                                visible: userRow.profile && userRow.profile.displayName !== userRow.modelData
                            }
                        }

                        Components.KomaiButton {
                            text: qsTr("Unignore")
                            icon.source: "qrc:/icons/icons/ui/dismiss.svg"
                            onClicked: userRow.profile.ignored = false
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                visible: TimelineManager.ignoredUsers.length === 0
                text: qsTr("You are not ignoring anyone.")
                color: palette.text
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }
    }
}
