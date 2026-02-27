// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import im.nheko 1.0

Dialog {
    id: root

    property string titleText: ""
    property string titleIcon: ""
    property color titleIconColor: palette.text
    property bool titleIconMirror: false
    property bool showCloseButton: true
    property int overlayDialogMinWidth: 520
    property real overlayDialogMaxWidthRatio: 0.8
    default property alias body: bodyLayout.data

    function overlayDialogWidth(dialogParent, contentImplicitWidth, dialogPadding)
    {
        const parentWidth = dialogParent ? dialogParent.width : 760;
        const viewportMax = Math.max(240, parentWidth - Nheko.paddingLarge * 2);
        const ratioMax = Math.max(240, Math.floor(parentWidth * overlayDialogMaxWidthRatio));
        const maxWidth = Math.min(viewportMax, ratioMax);
        const minWidth = Math.min(overlayDialogMinWidth, maxWidth);
        const contentWidth = Math.ceil(contentImplicitWidth + dialogPadding * 2);
        return Math.max(minWidth, Math.min(maxWidth, contentWidth));
    }

    modal: true
    padding: Nheko.paddingMedium
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    standardButtons: Dialog.NoButton
    width: overlayDialogWidth(parent, contentItem ? contentItem.implicitWidth : 0, padding)
    x: Math.round(((parent ? parent.width : width) - width) / 2)
    y: Math.round((parent ? parent.height : 0) / 4)

    // Workaround palettes not inheriting for popups
    palette: timelineRoot.palette
    parent: Overlay.overlay

    Overlay.modal: Rectangle {
        color: timelineRoot.overlayBackdropColor
    }

    background: Rectangle {
        color: palette.alternateBase
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: Nheko.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Nheko.paddingSmall

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: visible ? 24 : 0
                Layout.preferredHeight: visible ? 24 : 0
                fillMode: Image.PreserveAspectFit
                mirror: root.titleIconMirror
                source: root.titleIcon !== "" ? "image://colorimage/" + root.titleIcon + "?" + root.titleIconColor : ""
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
                visible: root.titleIcon !== ""
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                color: palette.text
                font.bold: true
                font.pointSize: Settings.uiFontSizePt * 1.2
                text: root.titleText
            }

            ImageButton {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/dismiss.svg"
                visible: root.showCloseButton

                onClicked: root.close()
            }
        }

        ColumnLayout {
            id: bodyLayout

            spacing: Nheko.paddingMedium
        }
    }
}
