// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Dialog {
    id: root

    property string titleIcon: ""
    property color titleIconColor: palette.text
    property bool titleIconMirror: false
    property bool showCloseButton: true
    property Item initialFocusItem: null
    property Item overlayViewport: null
    property int overlayDialogMinWidth: 520
    property real overlayDialogMaxWidthRatio: 0.8
    readonly property Item overlayDialogViewport: overlayViewport ? overlayViewport : parent
    readonly property int headerIconSize: Math.max(16, Math.ceil(headerFontMetrics.height))
    readonly property int overlayDialogChromeHeight: padding * 2 + contentItem.spacing + headerRow.implicitHeight
    default property alias body: bodyLayout.data

    onOpened: {
        if (initialFocusItem)
            initialFocusItem.forceActiveFocus();
        else
            root.forceActiveFocus();
    }

    function overlayDialogWidth(dialogParent, contentImplicitWidth, dialogPadding)
    {
        const parentWidth = dialogParent ? dialogParent.width : 760;
        const viewportMax = Math.max(240, parentWidth - Komai.paddingLarge * 2);
        const ratioMax = Math.max(240, Math.floor(parentWidth * overlayDialogMaxWidthRatio));
        const maxWidth = Math.min(viewportMax, ratioMax);
        const minWidth = Math.min(overlayDialogMinWidth, maxWidth);
        const contentWidth = Math.ceil(contentImplicitWidth + dialogPadding * 2);
        return Math.max(minWidth, Math.min(maxWidth, contentWidth));
    }

    // Suppress Dialog's built-in header; OverlayDialog renders its own title row.
    header: Item {}
    modal: true
    padding: Komai.paddingMedium
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    standardButtons: Dialog.NoButton
    width: overlayDialogWidth(overlayDialogViewport, contentItem ? contentItem.implicitWidth : 0, padding)
    x: Math.round(((overlayDialogViewport ? overlayDialogViewport.width : width) - width) / 2)
    y: Math.round((overlayDialogViewport ? overlayDialogViewport.height : 0) / 4)

    // Workaround palettes not inheriting for popups — Overlay.overlay
    // belongs to the Window, so its palette matches the application theme.
    palette: parent ? parent.palette : ({})
    parent: Overlay.overlay

    Overlay.modal: Rectangle {
        color: Qt.rgba(0.2, 0.2, 0.2, 0.7)
    }

    background: Rectangle {
        color: palette.alternateBase
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: Komai.paddingMedium

        FontMetrics {
            id: headerFontMetrics

            font.bold: true
            font.pointSize: Settings.uiFontSizePt * 1.2
        }

        RowLayout {
            id: headerRow

            Layout.fillWidth: true
            spacing: Komai.paddingSmall

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: visible ? root.headerIconSize : 0
                Layout.preferredHeight: visible ? root.headerIconSize : 0
                fillMode: Image.PreserveAspectFit
                mirror: root.titleIconMirror
                source: root.titleIcon !== "" ? "image://colorimage/" + root.titleIcon + "?" + root.titleIconColor : ""
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
                visible: root.titleIcon !== ""
                Accessible.ignored: true
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                color: palette.text
                font.bold: true
                font.pointSize: Settings.uiFontSizePt * 1.2
                text: root.title
            }

            ImageButton {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: root.headerIconSize
                Layout.preferredHeight: root.headerIconSize
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/dismiss.svg"
                visible: root.showCloseButton
                Accessible.name: qsTr("Close")
                Accessible.role: Accessible.Button

                onClicked: root.close()
            }
        }

        ColumnLayout {
            id: bodyLayout

            spacing: Komai.paddingMedium
        }
    }
}
