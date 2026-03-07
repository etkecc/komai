// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../ui" as Ui

RowLayout {
    id: root

    required property var model
    required property bool useStackedLayout

    readonly property string tagId: root.model.tagId ?? ""
    readonly property bool isAllRooms: root.tagId === ""
    readonly property bool hasShowToggle: !root.isAllRooms
    readonly property bool hasExcludeToggle: !root.isAllRooms
    property int _mutedRevision: 0
    property int _hiddenRevision: 0

    spacing: Komai.paddingSmall

    Connections {
        target: Communities
        function onMutedTagsChanged() { root._mutedRevision++; }
        function onHiddenTagsChanged() { root._hiddenRevision++; }
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

                ToolTip.visible: iconHover.hovered
                ToolTip.text: iconImg.toolTipText
                ToolTip.delay: 500

                HoverHandler {
                    id: iconHover
                }
            }

            Label {
                id: labelText
                visible: !root.useStackedLayout
                color: palette.text
                font.pointSize: 0.9 * Settings.uiFontSizePt
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

    // --- Mute toggle ---
    FilterToggleGroup {
        label: qsTr("Mute")
        iconSource: "image://colorimage/:/icons/icons/ui/volume-off-indicator.svg?" + palette.buttonText
        iconTooltip: qsTr("Do not show notification counts")

        toggle.checked: { void(root._mutedRevision); return Communities.isTagMuted(root.tagId); }
        toggle.onToggled: {
            Communities.toggleTagMute(root.tagId);
        }
    }

    // --- Exclude toggle ---
    FilterToggleGroup {
        visible: root.hasExcludeToggle
        label: qsTr("Exclude from 'All rooms'")
        iconSource: "image://colorimage/:/icons/icons/ui/globe-prohibited.svg?" + palette.buttonText
        iconTooltip: qsTr("Exclude from 'All rooms'")

        toggle.checked: { void(root._hiddenRevision); return Communities.isTagHidden(root.tagId); }
        toggle.onToggled: {
            Communities.toggleTagId(root.tagId);
        }
    }
}
