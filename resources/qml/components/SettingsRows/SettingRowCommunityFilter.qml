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

RowLayout {
    id: root

    required property var model
    required property bool useStackedLayout

    readonly property string tagId: root.model.tagId ?? ""
    readonly property bool isAllRooms: root.tagId === ""
    readonly property bool hasShowToggle: !root.isAllRooms
    readonly property bool hasExcludeToggle: !root.isAllRooms
    property int _badgesHiddenRevision: 0
    property int _hiddenRevision: 0

    spacing: Komai.paddingSmall

    Connections {
        target: Communities
        function onBadgesHiddenFiltersChanged() { root._badgesHiddenRevision++; }
        function onGlobalExcludesChanged() { root._hiddenRevision++; }
    }

    component FilterToggleGroup: Rectangle {
        id: group

        property alias label: labelText.text
        property alias iconSource: iconImg.source
        property alias iconTooltip: iconImg.toolTipText
        property alias toggle: toggleBtn

        implicitWidth: groupRow.implicitWidth + Komai.paddingSmall * 2
        implicitHeight: groupRow.implicitHeight + Komai.paddingSmall * 2
        color: Qt.rgba(palette.alternateBase.r, palette.alternateBase.g, palette.alternateBase.b, 0.5)
        border.color: Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.8)
        border.width: 1
        radius: Komai.paddingSmall

        RowLayout {
            id: groupRow
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

    // --- Show toggle ---
    FilterToggleGroup {
        visible: root.hasShowToggle
        label: qsTr("Show")
        iconSource: "image://colorimage/:/icons/icons/ui/eye-show.svg?" + palette.buttonText
        iconTooltip: qsTr("Show filter in sidebar")

        toggle.checked: root.model.value ?? false
        toggle.onToggled: {
            root.model.value = toggle.checked;
        }
    }

    // --- Attention badges toggle ---
    FilterToggleGroup {
        label: qsTr("Attention badges")
        iconSource: "image://colorimage/:/icons/icons/ui/counter.svg?" + palette.buttonText
        iconTooltip: qsTr("Badges indicate unread messages and unsent drafts")

        toggle.checked: { void(root._badgesHiddenRevision); return !Communities.areFilterBadgesHidden(root.tagId); }
        toggle.onToggled: {
            Communities.toggleFilterBadges(root.tagId);
        }
    }

    // --- Include in 'All rooms' toggle ---
    FilterToggleGroup {
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
