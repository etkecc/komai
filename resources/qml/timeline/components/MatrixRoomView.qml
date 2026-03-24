// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../room/components"
import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    required property var roomPreview
    required property bool showBackButton

    readonly property bool hasTimeline: TimelineManager.matrixTimelineItemCount > 0
    readonly property bool loading: TimelineManager.matrixTimelineLoading
    readonly property var composerShell: composerContainer
    property int lastPaginationTriggerCount: -1

    function focusTextInput() {
        composerInput.forceActiveFocus();
        return true;
    }

    function appendText(text) {
        if (!text)
            return false;

        composerInput.forceActiveFocus();
        composerInput.insert(composerInput.cursorPosition, text);
        return true;
    }

    function trySendMessage() {
        const body = composerInput.text;
        if (!TimelineManager.sendActiveMatrixTextMessage(body))
            return false;

        composerInput.text = "";
        composerInput.forceActiveFocus();
        return true;
    }

    anchors.fill: parent
    enabled: visible
    spacing: 0
    visible: !!roomPreview && roomPreview.isMatrixSummary

    RoomHeader {
        Layout.fillWidth: true
        room: null
        roomModel: null
        roomId: root.roomPreview ? root.roomPreview.roomid : ""
        roomName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarDisplayName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarUrl: root.roomPreview ? root.roomPreview.roomAvatarUrl : ""
        directChatOtherUserId: root.roomPreview ? root.roomPreview.directChatOtherUserId : ""
        isDirect: !!root.roomPreview && root.roomPreview.isDirect
        isEncrypted: !!root.roomPreview && root.roomPreview.isEncrypted
        roomTopic: root.roomPreview ? root.roomPreview.roomTopic : ""
        showBackButton: root.showBackButton
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: palette.mid
    }

    Rectangle {
        Layout.fillHeight: true
        Layout.fillWidth: true
        color: palette.base

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ListView {
                    id: matrixTimelineList

                    anchors.fill: parent
                    anchors.margins: Komai.paddingLarge
                    clip: true
                    model: TimelineManager.matrixTimelineModel
                    spacing: Komai.paddingMedium
                    visible: root.hasTimeline

                    onAtYBeginningChanged: {
                        if (atYBeginning
                                && root.hasTimeline
                                && !root.loading
                                && root.lastPaginationTriggerCount !== TimelineManager.matrixTimelineItemCount) {
                            if (TimelineManager.paginateActiveMatrixTimelineBackwards(0))
                                root.lastPaginationTriggerCount = TimelineManager.matrixTimelineItemCount;
                        }
                    }
                    onContentYChanged: {
                        if (contentY > 0 && root.lastPaginationTriggerCount === TimelineManager.matrixTimelineItemCount)
                            root.lastPaginationTriggerCount = -1;
                    }

                    delegate: Item {
                        required property string itemKind
                        required property string senderDisplayName
                        required property string senderAvatarUrl
                        required property string senderId
                        required property string body
                        required property double timestamp
                        required property bool isOwn

                        width: ListView.view.width
                        height: itemKind === "date_divider" ? dateDivider.implicitHeight : messageRow.implicitHeight

                        Rectangle {
                            id: dateDivider

                            anchors.horizontalCenter: parent.horizontalCenter
                            color: palette.mid
                            height: dividerLabel.implicitHeight + Komai.paddingSmall * 2
                            radius: height / 2
                            visible: itemKind === "date_divider"
                            width: dividerLabel.implicitWidth + Komai.paddingLarge * 2

                            MatrixText {
                                id: dividerLabel

                                anchors.centerIn: parent
                                color: palette.base
                                text: Qt.formatDateTime(new Date(timestamp), "dddd, d MMMM")
                                textFormat: TextEdit.PlainText
                            }
                        }

                        Item {
                            id: messageRow

                            anchors.left: parent.left
                            anchors.right: parent.right
                            implicitHeight: messageRowLayout.implicitHeight
                            visible: itemKind !== "date_divider"

                            RowLayout {
                                id: messageRowLayout

                                anchors.left: isOwn ? undefined : parent.left
                                anchors.right: isOwn ? parent.right : undefined
                                spacing: Komai.paddingSmall
                                width: Math.min(parent.width * 0.9, Math.max(320, parent.width * 0.7))

                                Avatar {
                                    Layout.alignment: Qt.AlignTop
                                    crop: true
                                    displayName: senderDisplayName
                                    implicitHeight: Komai.listIconSize
                                    implicitWidth: Komai.listIconSize
                                    roomid: root.roomPreview ? root.roomPreview.roomid : ""
                                    url: senderAvatarUrl.replace("mxc://", "image://MxcImage/")
                                    userid: senderId
                                    visible: !isOwn
                                }

                                Item {
                                    Layout.preferredWidth: visible ? Komai.paddingSmall : 0
                                    visible: !isOwn
                                }

                                ColumnLayout {
                                    Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                    Layout.preferredWidth: Math.min(messageRow.width * 0.82, Math.max(280, messageRow.width * 0.6))
                                    spacing: Komai.paddingSmall

                                    MatrixText {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: palette.buttonText
                                        text: senderDisplayName
                                        textFormat: TextEdit.PlainText
                                    }

                                    Rectangle {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: isOwn ? palette.highlight : palette.alternateBase
                                        implicitHeight: bubbleBody.implicitHeight + Komai.paddingMedium * 2
                                        implicitWidth: Math.min(parent.width, bubbleBody.implicitWidth + Komai.paddingLarge * 2)
                                        radius: Komai.paddingMedium * 2

                                        MatrixText {
                                            id: bubbleBody

                                            anchors.fill: parent
                                            anchors.margins: Komai.paddingMedium
                                            color: isOwn ? palette.highlightedText : palette.text
                                            text: body
                                            textFormat: TextEdit.PlainText
                                            width: parent.width - Komai.paddingMedium * 2
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    MatrixText {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: palette.buttonText
                                        text: Qt.formatTime(new Date(timestamp), "h:mm ap")
                                        textFormat: TextEdit.PlainText
                                    }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Komai.paddingMedium
                    visible: !root.hasTimeline
                    width: Math.min(parent.width - Komai.paddingLarge * 2, 560)

                    MatrixText {
                        Layout.fillWidth: true
                        horizontalAlignment: TextEdit.AlignHCenter
                        text: root.loading
                            ? qsTr("Loading room timeline…")
                            : qsTr("No timeline items are loaded for this room yet.")
                        wrapMode: Text.WordWrap
                    }

                    MatrixText {
                        Layout.fillWidth: true
                        color: palette.buttonText
                        horizontalAlignment: TextEdit.AlignHCenter
                        text: qsTr("This room is now backed by the Rust matrix-sdk timeline. Plain text sending is available while richer composer features are migrated.")
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                id: composerContainer

                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                color: palette.window
                implicitHeight: composerLayout.implicitHeight + Komai.paddingMedium * 2

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    color: palette.mid
                    height: 1
                }

                ColumnLayout {
                    id: composerLayout

                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingSmall

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(Math.max(56, composerInput.contentHeight + Komai.paddingMedium * 2), 160)
                        Layout.maximumHeight: 160
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        contentWidth: availableWidth
                        padding: 0

                        Components.KomaiTextArea {
                            id: composerInput

                            placeholderText: qsTr("Write a message…")
                            selectByMouse: true
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.Wrap

                            Keys.onPressed: event => {
                                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                        && !(event.modifiers & Qt.ShiftModifier)
                                        && !(event.modifiers & Qt.ControlModifier)
                                        && !(event.modifiers & Qt.AltModifier)
                                        && !(event.modifiers & Qt.MetaModifier)) {
                                    event.accepted = root.trySendMessage();
                                }
                            }

                            background: Rectangle {
                                border.color: palette.mid
                                border.width: 1
                                color: palette.base
                                radius: Komai.paddingMedium
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        MatrixText {
                            Layout.fillWidth: true
                            color: palette.buttonText
                            text: qsTr("Shift+Enter inserts a newline. Rich replies, uploads, and other composer tools will move here as the Rust room view grows.")
                            wrapMode: Text.WordWrap
                        }

                        Components.KomaiButton {
                            enabled: composerInput.text.trim().length > 0
                            text: qsTr("Send")

                            onClicked: root.trySendMessage()
                        }
                    }
                }
            }
        }
    }

    Connections {
        function onFocusInput() {
            root.focusTextInput();
        }

        target: TimelineManager
    }
}
