// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property string renderedRawMessage: ""
    property string rawMessageJson: ""
    property string rawMessageBody: ""
    property string rawMessageFormattedBody: ""
    property string copiedField: ""
    readonly property bool hasRawMessageBody: rawMessageBody !== ""
    readonly property bool hasRawMessageFormattedBody: rawMessageFormattedBody !== ""

    function copyField(field, value)
    {
        Clipboard.text = value;
        copiedField = field;
        copyFeedbackTimer.restart();
    }

    overlayDialogMinWidth: 680
    overlayDialogMaxWidthRatio: 0.92
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    titleText: qsTr("Raw message inspection")
    titleIcon: ":/icons/icons/ui/raw-message.svg"
    titleIconColor: palette.text

    Shortcut {
        enabled: root.visible
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }

    ScrollView {
        id: rawMessageScrollView

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 440
        padding: Komai.paddingSmall

        TextArea {
            id: rawMessageView

            font: Komai.monospaceFont()
            color: palette.text
            readOnly: true
            selectByMouse: true
            text: root.renderedRawMessage
            textFormat: TextEdit.RichText
            wrapMode: TextEdit.NoWrap

            width: rawMessageScrollView.availableWidth
            height: Math.max(rawMessageScrollView.availableHeight, implicitHeight)

            background: Rectangle {
                color: palette.base
                radius: 6
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        Button {
            icon.source: root.copiedField === "all" ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            icon.width: 16
            icon.height: 16
            text: root.copiedField === "all" ? qsTr("Copied") : qsTr("Copy All")
            onClicked: root.copyField("all", root.rawMessageJson)
        }

        Button {
            visible: root.hasRawMessageBody
            icon.source: root.copiedField === "body" ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            icon.width: 16
            icon.height: 16
            text: root.copiedField === "body" ? qsTr("Copied") : qsTr("Copy Body")
            onClicked: root.copyField("body", root.rawMessageBody)
        }

        Button {
            visible: root.hasRawMessageFormattedBody
            icon.source: root.copiedField === "formattedBody" ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            icon.width: 16
            icon.height: 16
            text: root.copiedField === "formattedBody" ? qsTr("Copied") : qsTr("Copy Formatted Body")
            onClicked: root.copyField("formattedBody", root.rawMessageFormattedBody)
        }

        Item {
            Layout.fillWidth: true
        }

        Button {
            icon.source: "qrc:/icons/icons/ui/checkmark.svg"
            icon.width: 18
            icon.height: 18
            text: qsTr("OK")
            highlighted: true
            onClicked: root.close()
        }
    }

    Timer {
        id: copyFeedbackTimer

        interval: 2000
        onTriggered: root.copiedField = ""
    }
}
