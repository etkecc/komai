// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import ".." as Components
import "../../ui" as Ui

ColumnLayout {
    id: root

    property int _hiddenSpacesRevision: 0
    property int _badgesHiddenRevision: 0
    property int _globalExcludesRevision: 0

    Connections {
        target: Communities
        function onHiddenSpacesChanged() { root._hiddenSpacesRevision++; }
        function onBadgesHiddenFiltersChanged() { root._badgesHiddenRevision++; }
        function onGlobalExcludesChanged() { root._globalExcludesRevision++; }
    }

    readonly property var spacesList: Communities.spaceEntries()

    spacing: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium

    // "No spaces" message
    Label {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingSmall
        visible: root.spacesList.length === 0
        text: qsTr("No spaces found. Join a space to see it here.")
        color: palette.buttonText
        font.pointSize: Settings.uiFontSizePt
        wrapMode: Text.Wrap
    }

    // Space entries
    Repeater {
        model: root.spacesList

        delegate: Item {
            id: spaceCard

            required property var modelData
            readonly property string spaceId: modelData.id
            readonly property string spaceName: modelData.name || spaceId
            readonly property string spaceAvatar: modelData.avatarUrl

            Layout.fillWidth: true
            implicitHeight: spaceCardBg.implicitHeight

            HoverHandler {
                id: spaceCardHover
                blocking: false
            }

            Rectangle {
                id: spaceCardBg
                anchors.left: parent.left
                anchors.right: parent.right
                implicitHeight: spaceCardRow.implicitHeight + Komai.paddingMedium * 2
                color: spaceCardHover.hovered ? palette.dark : palette.window
                border.color: Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.4)
                border.width: 1
                radius: Komai.paddingMedium

                RowLayout {
                    id: spaceCardRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Komai.paddingSmall
                    anchors.rightMargin: Komai.paddingSmall
                    spacing: Komai.paddingMedium

                    // Space avatar
                    Avatar {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: Komai.iconSize
                        Layout.preferredHeight: Komai.iconSize
                        displayName: spaceCard.spaceName
                        enabled: false
                        roomid: spaceCard.spaceId
                        textColor: spaceCardHover.hovered ? palette.brightText : palette.text
                        url: {
                            if (spaceCard.spaceAvatar.startsWith("mxc://"))
                                return spaceCard.spaceAvatar.replace("mxc://", "image://MxcImage/");
                            else if (spaceCard.spaceAvatar.length > 0)
                                return spaceCard.spaceAvatar;
                            else
                                return "";
                        }
                    }

                    // Space name + badge
                    Item {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        implicitHeight: spaceNameRow.implicitHeight

                        Row {
                            id: spaceNameRow
                            width: parent.width
                            spacing: Komai.paddingSmall
                            clip: true

                            Rectangle {
                                id: spaceBadgeRect
                                readonly property color badgeColor: spaceCardHover.hovered ? palette.brightText : palette.buttonText
                                anchors.verticalCenter: parent.verticalCenter
                                implicitWidth: spaceBadgeLabel.implicitWidth + Komai.paddingSmall * 2
                                implicitHeight: spaceBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                                radius: Komai.paddingSmall
                                color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
                                border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
                                border.width: 1

                                Label {
                                    id: spaceBadgeLabel
                                    anchors.centerIn: parent
                                    text: qsTr("Space")
                                    color: spaceBadgeRect.badgeColor
                                    font.pointSize: Settings.uiFontSizePt * 0.8
                                }
                            }

                            Text {
                                width: Math.min(implicitWidth, spaceNameRow.width - spaceBadgeRect.width - spaceNameRow.spacing)
                                anchors.verticalCenter: parent.verticalCenter
                                text: spaceCard.spaceName
                                color: spaceCardHover.hovered ? palette.brightText : palette.text
                                font.pointSize: 1.1 * Settings.uiFontSizePt
                                elide: Text.ElideRight
                            }
                        }
                    }

                    // Controls
                    Flow {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Komai.paddingSmall

                        // Show toggle
                        SpaceTogglePill {
                            label: qsTr("Show")
                            iconSource: "image://colorimage/:/icons/icons/ui/eye-show.svg?" + palette.buttonText
                            iconTooltip: qsTr("Show space in sidebar")
                            toggle.checked: { void(root._hiddenSpacesRevision); return !Communities.isSpaceHidden(spaceCard.spaceId); }
                            toggle.onToggled: Communities.toggleSpaceHidden(spaceCard.spaceId)
                        }

                        // Attention badges toggle
                        SpaceTogglePill {
                            label: qsTr("Attention badges")
                            iconSource: "image://colorimage/:/icons/icons/ui/counter.svg?" + palette.buttonText
                            iconTooltip: qsTr("Badges indicate unread messages and unsent drafts")
                            toggle.checked: { void(root._badgesHiddenRevision); return !Communities.areFilterBadgesHidden(spaceCard.spaceId); }
                            toggle.onToggled: Communities.toggleFilterBadges(spaceCard.spaceId)
                        }

                        // Include in 'All rooms' toggle
                        SpaceTogglePill {
                            label: qsTr("Include in 'All rooms'")
                            iconSource: "image://colorimage/:/icons/icons/ui/globe.svg?" + palette.buttonText
                            iconTooltip: qsTr("Include in 'All rooms'")
                            toggle.checked: { void(root._globalExcludesRevision); return !Communities.isGlobalExcluded(spaceCard.spaceId); }
                            toggle.onToggled: Communities.toggleGlobalExclude(spaceCard.spaceId)
                        }
                    }
                }
            }
        }
    }

    // Toggle pill matching SettingRowCommunityFilter's FilterToggleGroup style
    component SpaceTogglePill: Rectangle {
        id: pill

        property alias label: labelText.text
        property alias iconSource: iconImg.source
        property alias iconTooltip: iconImg.toolTipText
        property alias toggle: toggleBtn

        implicitWidth: pillRow.implicitWidth + Komai.paddingSmall * 2
        implicitHeight: pillRow.implicitHeight + Komai.paddingSmall * 2
        color: Qt.rgba(palette.alternateBase.r, palette.alternateBase.g, palette.alternateBase.b, 0.5)
        border.color: Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.8)
        border.width: 1
        radius: Komai.paddingSmall

        RowLayout {
            id: pillRow
            anchors.centerIn: parent
            spacing: Komai.paddingSmall

            Image {
                id: iconImg
                sourceSize.width: toggleBtn.height * 0.5
                sourceSize.height: toggleBtn.height * 0.5
                Layout.alignment: Qt.AlignVCenter

                property string toolTipText

                HoverHandler {
                    id: iconHover
                }

                Components.KomaiToolTip {
                    anchorItem: iconImg
                    anchorX: iconImg.width / 2
                    anchorY: 0
                    text: iconImg.toolTipText
                    delay: 500
                    requestedVisible: iconHover.hovered && iconImg.toolTipText.length > 0
                }
            }

            Label {
                id: labelText
                color: palette.text
                font.pointSize: Settings.uiFontSizePt
            }

            Ui.ToggleButton {
                id: toggleBtn
            }
        }
    }
}
