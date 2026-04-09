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

Item {
    id: root

    required property var model
    required property bool useStackedLayout

    readonly property string tagId: root.model.tagId ?? ""
    readonly property bool isAllRooms: root.tagId === ""
    readonly property bool hasShowToggle: !root.isAllRooms
    readonly property bool hasExcludeToggle: !root.isAllRooms
    property int _badgesHiddenRevision: 0
    property int _hiddenRevision: 0

    implicitHeight: cardBg.implicitHeight

    Connections {
        target: Communities
        function onBadgesHiddenFiltersChanged() { root._badgesHiddenRevision++; }
        function onGlobalExcludesChanged() { root._hiddenRevision++; }
    }

    HoverHandler {
        id: cardHover
        blocking: false
    }

    Rectangle {
        id: cardBg
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: cardRow.implicitHeight + Komai.paddingMedium * 2
        color: cardHover.hovered ? palette.dark : palette.window
        border.color: Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.4)
        border.width: 1
        radius: Komai.paddingMedium

        RowLayout {
            id: cardRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Komai.paddingSmall
            anchors.rightMargin: Komai.paddingSmall
            spacing: Komai.paddingMedium

            // Large icon
            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: Komai.listIconSize
                Layout.preferredHeight: Komai.listIconSize
                source: root.model.icon
                    ? "image://colorimage/" + root.model.icon + "?" + (cardHover.hovered ? palette.brightText : palette.buttonText)
                    : ""
                sourceSize.width: Komai.listIconSize
                sourceSize.height: Komai.listIconSize
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: (root.model.icon ?? "").length > 0
            }

            // Name + description
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: root.model.name ?? ""
                    color: cardHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: (root.model.description ?? "").length > 0
                    text: root.model.description ?? ""
                    textFormat: Text.RichText
                    color: cardHover.hovered ? palette.brightText : palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                    wrapMode: Text.Wrap
                    onLinkActivated: function(link) { Qt.openUrlExternally(link); }
                }
            }

            // Controls
            Flow {
                Layout.alignment: Qt.AlignVCenter
                spacing: Komai.paddingSmall

                // --- Show toggle ---
                FilterTogglePill {
                    visible: root.hasShowToggle
                    label: qsTr("Show")
                    iconSource: "image://colorimage/:/icons/icons/ui/eye-show.svg?" + palette.buttonText
                    iconTooltip: qsTr("Show filter in sidebar")

                    toggle.checked: root.model.value ?? false
                    toggle.onToggled: {
                        root.model.value = toggle.checked;
                    }
                    toggle.enabled: root.model.enabled
                }

                // --- Attention badges toggle ---
                FilterTogglePill {
                    label: qsTr("Attention badges")
                    iconSource: "image://colorimage/:/icons/icons/ui/counter.svg?" + palette.buttonText
                    iconTooltip: qsTr("Badges indicate unread messages and unsent drafts")

                    toggle.checked: { void(root._badgesHiddenRevision); return !Communities.areFilterBadgesHidden(root.tagId); }
                    toggle.onToggled: {
                        Communities.toggleFilterBadges(root.tagId);
                    }
                }

                // --- Include in 'All rooms' toggle ---
                FilterTogglePill {
                    visible: root.hasExcludeToggle
                    label: qsTr("Include in 'All rooms'")
                    iconSource: "image://colorimage/:/icons/icons/ui/globe.svg?" + palette.buttonText
                    iconTooltip: qsTr("Include in 'All rooms'")

                    toggle.checked: { void(root._hiddenRevision); return !Communities.isGlobalExcluded(root.tagId); }
                    toggle.onToggled: {
                        Communities.toggleGlobalExclude(root.tagId);
                    }
                }
            }
        }
    }

    component FilterTogglePill: Rectangle {
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
                visible: !root.useStackedLayout
                color: palette.text
                font.pointSize: Settings.uiFontSizePt
            }

            Ui.ToggleButton {
                id: toggleBtn
                enabled: root.model.enabled
            }
        }
    }
}
