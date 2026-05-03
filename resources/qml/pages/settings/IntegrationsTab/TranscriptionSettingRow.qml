// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

// Reusable single-setting card matching the visual idiom of the
// model-driven settings rows (label + optional description on the left,
// control area on the right, hover-aware background, stacks at narrow
// widths). Used by `TranscriptionSetting.qml` to lay out every voice-
// transcription setting as its own card in `Settings → Integrations`.
Item {
    id: root

    required property string label
    property string description: ""
    property real controlWidth: useStackedLayout
        ? Math.max(0, width - Komai.paddingSmall * 2)
        : Math.min(600, Math.max(240, width - Komai.paddingLarge * 2))
    default property alias controlContent: controlContainer.data

    readonly property bool useStackedLayout: width < Komai.settingRowStackBreakpoint
    readonly property bool hasDescription: description.length > 0
    readonly property bool mirrored: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    // Surfaced so child controls can flip their colors when the surrounding
    // card is hovered (the card's background goes to `palette.dark`, which
    // would otherwise drown out `palette.buttonText`-coloured controls).
    readonly property alias hovered: rowHover.hovered

    Layout.fillWidth: true
    implicitHeight: rowColumn.implicitHeight
    implicitWidth: rowColumn.implicitWidth

    HoverHandler {
        id: rowHover
        blocking: false
    }

    Rectangle {
        anchors.fill: rowColumn
        color: rowHover.hovered ? palette.dark : palette.window
        radius: Komai.paddingMedium
        z: -1
    }

    ColumnLayout {
        id: rowColumn
        width: root.width
        spacing: Komai.paddingMedium

        GridLayout {
            id: settingRow
            Layout.fillWidth: true
            Layout.topMargin: (Komai.density !== Settings.Density.Spacious)
                ? Komai.paddingSmall
                : Komai.paddingMedium
            Layout.leftMargin: Komai.paddingSmall
            Layout.rightMargin: Komai.paddingSmall
            Layout.bottomMargin: root.hasDescription ? 0 : ((Komai.density !== Settings.Density.Spacious)
                ? Komai.paddingSmall
                : Komai.paddingMedium)
            columns: root.useStackedLayout ? 1 : 2
            rowSpacing: root.useStackedLayout ? Komai.paddingSmall : 0
            columnSpacing: Komai.paddingSmall

            Text {
                Layout.row: 0
                Layout.column: 0
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.rightMargin: root.useStackedLayout ? 0 : Komai.paddingSmall
                color: rowHover.hovered ? palette.brightText : palette.text
                text: root.label
                textFormat: Text.AutoText
                font.pointSize: 1.1 * Settings.uiFontSizePt
                horizontalAlignment: root.mirrored ? Text.AlignRight : Text.AlignLeft
                LayoutMirroring.enabled: false
                wrapMode: Text.Wrap
            }

            Item {
                id: controlContainer
                Layout.row: root.useStackedLayout ? 1 : 0
                Layout.column: root.useStackedLayout ? 0 : 1
                Layout.alignment: (root.useStackedLayout ? Qt.AlignLeft : Qt.AlignRight) | Qt.AlignTop
                Layout.fillWidth: root.useStackedLayout
                Layout.preferredWidth: root.controlWidth
                Layout.maximumWidth: root.controlWidth
                Layout.minimumWidth: root.useStackedLayout ? 0 : 140
                readonly property real childImplicitHeight: children.length > 0 && children[0]
                    ? Math.max(
                          children[0].implicitHeight || 0,
                          children[0].height || 0,
                          children[0].contentHeight || 0)
                    : 0
                Layout.preferredHeight: childImplicitHeight
                Layout.minimumHeight: childImplicitHeight
            }
        }

        TextEdit {
            Layout.fillWidth: true
            Layout.leftMargin: Komai.paddingSmall
            Layout.rightMargin: Komai.paddingSmall
            Layout.topMargin: -Komai.paddingSmall
            Layout.bottomMargin: (Komai.density !== Settings.Density.Spacious)
                ? Komai.paddingSmall
                : Komai.paddingMedium
            visible: root.hasDescription
            color: rowHover.hovered ? palette.brightText : palette.buttonText
            text: root.description
            textFormat: Text.RichText
            font.pointSize: Settings.uiFontSizePt
            horizontalAlignment: root.mirrored ? Text.AlignRight : Text.AlignLeft
            LayoutMirroring.enabled: false
            wrapMode: Text.Wrap
            readOnly: true
            selectByMouse: true
            onLinkActivated: function(link) {
                Qt.openUrlExternally(link);
            }
        }
    }
}
