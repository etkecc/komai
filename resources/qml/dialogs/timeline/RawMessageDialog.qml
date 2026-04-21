// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    // Inputs from the C++ side. Each segment receives a pre-rendered
    // syntax-highlighted HTML blob (`*Rendered`) plus the underlying raw
    // JSON string for the "Copy" buttons. Either segment may instead be
    // empty with a non-empty `*Error` field — the dialog handles both.
    property string cleartextRendered: ""
    property string cleartextJson: ""
    property string cleartextError: ""
    property string wireRendered: ""
    property string wireJson: ""
    property string wireError: ""
    // True when the wire form is byte-equivalent to the cleartext (i.e. the
    // event was sent in the clear). Drives the "(same)" annotation on the
    // wire-form segment so users know upfront they don't need to switch tabs.
    property bool wireMatchesCleartext: false
    // Body / formatted_body extracted from the cleartext payload. Used for
    // the "Copy `body`" / "Copy `formatted_body`" shortcut buttons.
    property string rawMessageBody: ""
    property string rawMessageFormattedBody: ""
    property string copiedField: ""

    readonly property bool hasRawMessageBody: rawMessageBody !== ""
    readonly property bool hasRawMessageFormattedBody: rawMessageFormattedBody !== ""

    // 0 = cleartext, 1 = wire. Cleartext is the default — that's what the user
    // typically wants to see ("what does this message say?"). The wire form is
    // a power-user / verification view.
    property int currentSegment: 0
    readonly property bool showingCleartext: currentSegment === 0
    readonly property bool showingWire: currentSegment === 1

    readonly property string activeRendered: showingCleartext ? cleartextRendered : wireRendered
    readonly property string activeError: showingCleartext ? cleartextError : wireError
    readonly property string activeJson: showingCleartext ? cleartextJson : wireJson

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

    SegmentedButton {
        id: formSegment

        Layout.fillWidth: true
        implicitHeight: Math.max(40, Math.round(Settings.uiFontSizePt * 3))
        currentIndex: root.currentSegment
        // Keep tab labels conceptual ("Cleartext" works whether decryption was
        // needed or not). Annotate the wire segment with "(same)" when the two
        // forms match, so users know switching wouldn't reveal anything new.
        model: [
            { text: qsTr("Cleartext") },
            {
                text: root.wireMatchesCleartext
                    ? qsTr("Wire form (same)")
                    : qsTr("Wire form")
            }
        ]
        onActivated: function(index) {
            root.currentSegment = index;
            // Reset copy feedback when switching — the button now copies a
            // different payload, so the previous "Copied" badge is misleading.
            root.copiedField = "";
        }
    }

    ScrollView {
        id: rawMessageScrollView

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 440
        padding: Komai.paddingSmall
        // When an error is present for the active segment, show it as plain
        // text instead of the (empty) syntax-highlighted block. UTDs land here
        // for the cleartext segment; server-fetch failures land here for the
        // wire segment.
        visible: root.activeError === ""

        Components.KomaiTextArea {
            id: rawMessageView

            font: Komai.monospaceFont()
            readOnly: true
            text: root.activeRendered
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

    // Error banner — replaces the JSON view when the active segment has
    // nothing to render (UTD on cleartext, server fetch failure on wire).
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: errorLabel.implicitHeight + Komai.paddingMedium * 2
        visible: root.activeError !== ""
        radius: 6
        color: palette.base

        Label {
            id: errorLabel
            anchors.fill: parent
            anchors.margins: Komai.paddingMedium
            text: root.activeError
            color: palette.text
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        Components.KomaiButton {
            // Copy the active segment's JSON. Disabled when the segment is in
            // its error state (nothing to copy).
            enabled: root.activeJson !== ""
            icon.source: root.copiedField === "all" ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            text: root.copiedField === "all" ? qsTr("Copied") : qsTr("Copy All")
            onClicked: root.copyField("all", root.activeJson)
        }

        Components.KomaiButton {
            // body / formatted_body shortcut buttons only make sense for the
            // cleartext segment — the wire form for an encrypted event has
            // ciphertext fields (`algorithm`, `ciphertext`, …) instead.
            visible: root.showingCleartext && root.hasRawMessageBody
            icon.source: root.copiedField === "body" ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            text: root.copiedField === "body" ? qsTr("Copied") : qsTr("Copy `body`")
            onClicked: root.copyField("body", root.rawMessageBody)
        }

        Components.KomaiButton {
            visible: root.showingCleartext && root.hasRawMessageFormattedBody
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
