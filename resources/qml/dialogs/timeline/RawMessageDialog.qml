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
    title: qsTr("Raw message inspection")
    titleIcon: ":/icons/icons/ui/raw-message.svg"
    titleIconColor: palette.text

    Shortcut {
        enabled: root.visible
        sequences: [StandardKey.Cancel, "Escape"]
        context: Qt.ApplicationShortcut
        onActivated: root.close()
        onActivatedAmbiguously: root.close()
    }

    ScrollView {
        id: rawMessageScrollView

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 440
        padding: Komai.paddingSmall

        Components.KomaiTextArea {
            id: rawMessageView

            font: Komai.monospaceFont()
            readOnly: true
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

        Components.KomaiButton {
            icon.source: root.copiedField === "all" ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            text: root.copiedField === "all" ? qsTr("Copied") : qsTr("Copy All")
            onClicked: root.copyField("all", root.rawMessageJson)
        }

        Components.KomaiButton {
            visible: root.hasRawMessageBody
            icon.source: root.copiedField === "body" ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            text: root.copiedField === "body" ? qsTr("Copied") : qsTr("Copy `body`")
            onClicked: root.copyField("body", root.rawMessageBody)
        }

        Components.KomaiButton {
            visible: root.hasRawMessageFormattedBody
            icon.source: root.copiedField === "formattedBody" ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            text: root.copiedField === "formattedBody" ? qsTr("Copied") : qsTr("Copy `formatted_body`")
            onClicked: root.copyField("formattedBody", root.rawMessageFormattedBody)
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            icon.source: "qrc:/icons/icons/ui/checkmark.svg"
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
