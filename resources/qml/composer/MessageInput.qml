// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../emoji"
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import QtQuick.Window 2.13
import cc.etke.komai 1.0

Rectangle {
    id: inputBar

    required property var room
    required property var timelineRoot
    property bool showAllButtons: width > 450 || (messageInput.length == 0 && !messageInput.inputMethodComposing)
    readonly property string text: messageInput.text
    readonly property bool hasUploads: room && room.input.uploads.length > 0
    readonly property bool composerEnabled: !hasUploads
    readonly property bool hasSendableContent: messageInput.length > 0 || hasUploads

    Layout.fillWidth: true
    Layout.minimumHeight: 48
    Layout.preferredHeight: row.implicitHeight
    color: palette.window

    RowLayout {
        id: row

        anchors.fill: parent
        spacing: 0
        visible: room ? room.permissions.canSend(room.isEncrypted ? MtxEvent.Encrypted :  MtxEvent.TextMessage) : false

        ComposerCallButton {
            room: inputBar.room
            timelineRoot: inputBar.timelineRoot
            showAllButtons: inputBar.showAllButtons
        }
        ComposerAttachButton {
            room: inputBar.room
            showAllButtons: inputBar.showAllButtons
        }
        ScrollView {
            id: textInput

            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.maximumHeight: Window.height / 4
            Layout.minimumHeight: visible ? fontMetrics.lineSpacing : 0
            Layout.preferredHeight: visible ? contentHeight : 0
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            contentWidth: availableWidth

            TextArea {
                id: messageInput

                property int completerTriggeredAt: 0
                property string lastChar
                property int previousTextLength: 0

                function insertCompletion(completion) {
                    messageInput.remove(completerTriggeredAt, cursorPosition);
                    messageInput.insert(cursorPosition, completion);
                    let userid = completer.currentUserid();
                    if (userid) {
                        room.input.addMention(userid, completion);
                    }
                }
                function openCompleter(pos, type) {
                    completerTriggeredAt = pos;
                    completer.completerType = type;
                    if (!popup.opened)
                        popup.open();
                    completer.completer.setSearchString(messageInput.getText(completerTriggeredAt, cursorPosition) + messageInput.preeditText);
                }
                function completerTypeForTrigger(trigger, tokenStart) {
                    if ((trigger === '@' || trigger === '＠') && Settings.composerInputInlineUserPickerEnabled)
                        return "user";
                    if ((trigger === ':' || trigger === '：') && Settings.composerInputInlineEmojiPickerEnabled)
                        return "emoji";
                    if ((trigger === '#' || trigger === '＃') && Settings.composerInputInlineRoomPickerEnabled)
                        return "roomAliases";
                    if (trigger === '~' || trigger === '～')
                        return "customEmoji";
                    if ((trigger === '/' || trigger === '／') && tokenStart === 0)
                        return "command";
                    return "";
                }
                function positionCursorAtEnd() {
                    cursorPosition = messageInput.length;
                }
                function positionCursorAtStart() {
                    cursorPosition = 0;
                }
                function maybeOpenCompleterForTrailingTokenAfterBulkInsert() {
                    if (popup.opened || cursorPosition !== text.length)
                        return;

                    var tokenStart = cursorPosition - 1;
                    while (tokenStart >= 0) {
                        const c = text.charAt(tokenStart);
                        if (c === ' ' || c === '\t' || c === '\n')
                            break;
                        tokenStart = tokenStart - 1;
                    }
                    tokenStart = tokenStart + 1;

                    if (tokenStart < 0 || tokenStart >= cursorPosition)
                        return;

                    const tokenLength = cursorPosition - tokenStart;
                    if (tokenLength < 2)
                        return;

                    const trigger = text.charAt(tokenStart);
                    const type = messageInput.completerTypeForTrigger(trigger, tokenStart);
                    if (type !== "")
                        messageInput.openCompleter(tokenStart, type);
                }

                background: null
                bottomPadding: 8
                color: palette.text
                enabled: inputBar.composerEnabled
                focus: true
                leftPadding: inputBar.showAllButtons ? 0 : 8
                padding: 0
                font.pointSize: Settings.uiFontSizePt
                placeholderText: qsTr("Write a message...")
                placeholderTextColor: palette.buttonText
                selectByMouse: true
                topPadding: 8
                verticalAlignment: TextEdit.AlignVCenter
                width: textInput.width
                wrapMode: TextEdit.Wrap

                Keys.onPressed: event => {
                    if (event.modifiers === (Qt.ControlModifier | Qt.ShiftModifier) && event.key === Qt.Key_V) {
                        // Ctrl+Shift+V: paste as plain text (Qt doesn't handle this natively,
                        // and the unhandled key event produces a control character / tofu square)
                        var clipText = room.input.clipboardText();
                        if (clipText)
                            messageInput.insert(messageInput.cursorPosition, clipText);
                        event.accepted = true;
                    } else if (event.matches(StandardKey.Paste)) {
                        event.accepted = room.input.tryPasteAttachment(false);
                    } else if (event.key == Qt.Key_Space) {
                        // close popup if user enters space after colon
                        if (cursorPosition == completerTriggeredAt + 1)
                            popup.close();
                        if (popup.opened && completer.count <= 0)
                            popup.close();
                    } else if (event.modifiers == Qt.ControlModifier && event.key == Qt.Key_U) {
                        messageInput.clear();
                    } else if (event.modifiers == Qt.ControlModifier && event.key == Qt.Key_P) {
                        messageInput.text = room.input.previousText();
                    } else if (event.modifiers == Qt.ControlModifier && event.key == Qt.Key_N) {
                        messageInput.text = room.input.nextText();
                    } else if (event.key == Qt.Key_Escape && popup.opened) {
                        completer.completerType = "";
                        popup.close();
                        event.accepted = true;
                    } else if (event.matches(StandardKey.SelectAll) && popup.opened) {
                        completer.completerType = "";
                        popup.close();
                    } else if (event.key == Qt.Key_Enter || event.key == Qt.Key_Return) {
                        // If popup is open and user has selected a completion, insert it.
                        if (popup.opened &&
                            (event.modifiers == Qt.NoModifier
                            || event.modifiers == Qt.ShiftModifier
                            || event.modifiers == Qt.ControlModifier)
                        ) {
                            var currentCompletion = completer.currentCompletion();
                            let userid = completer.currentUserid();

                            completer.completerType = "";
                            popup.close();

                            if (currentCompletion) {
                                messageInput.insertCompletion(currentCompletion);
                                if (userid) {
                                    console.log(userid);
                                    room.input.addMention(userid, currentCompletion);
                                }
                                event.accepted = true;
                            }
                            // Nothing selected: popup closed, fall through to send/newline.
                        }
                        // Send message Enter key combination event.
                        if (!event.accepted && (
                            Settings.composerInputSendKey == 0 && event.modifiers == Qt.NoModifier
                              || Settings.composerInputSendKey == 1 && event.modifiers == Qt.ShiftModifier
                              || Settings.composerInputSendKey == 2 && event.modifiers == Qt.ControlModifier)
                        ) {
                            room.input.send();
                            event.accepted = true;
                        }
                        // Add newline Enter key combination event.
                        else if (!event.accepted && (
                            Settings.composerInputSendKey == 0 && event.modifiers == Qt.ShiftModifier
                              || Settings.composerInputSendKey == 1 && event.modifiers == Qt.NoModifier
                              || Settings.composerInputSendKey == 2 && event.modifiers == Qt.ShiftModifier)
                        ) {
                            messageInput.insert(messageInput.cursorPosition, "\n");
                            event.accepted = true;
                        }
                        // Any other Enter key combo is ignored here.
                    } else if (event.key == Qt.Key_Tab && (event.modifiers == Qt.NoModifier || event.modifiers == Qt.ShiftModifier)) {
                        event.accepted = true;
                        if (popup.opened) {
                            if (event.modifiers & Qt.ShiftModifier)
                                completer.down();
                            else
                                completer.up();
                        } else {
                            var pos = cursorPosition - 1;
                            while (pos > -1) {
                                var t = messageInput.getText(pos, pos + 1);
                                console.log('"' + t + '"');
                                const type = messageInput.completerTypeForTrigger(t, pos);
                                if (type !== "") {
                                    messageInput.openCompleter(pos, type);
                                    return;
                                } else if (t == ' ' || t == '\t') {
                                    messageInput.openCompleter(pos + 1, "user");
                                    return;
                                }
                                pos = pos - 1;
                            }
                            // At start of input
                            messageInput.openCompleter(0, "user");
                        }
                    } else if (event.key == Qt.Key_Up && popup.opened) {
                        event.accepted = true;
                        completer.up();
                    } else if ((event.key == Qt.Key_Down || event.key == Qt.Key_Backtab) && popup.opened) {
                        event.accepted = true;
                        completer.down();
                    } else if (event.key == Qt.Key_Up && (event.modifiers == Qt.NoModifier || event.modifiers == Qt.KeypadModifier)) {
                        if (cursorPosition == 0) {
                            event.accepted = true;
                            var idx = room.edit ? room.idToIndex(room.edit) + 1 : 0;
                            while (true) {
                                var id = room.indexToId(idx);
                                if (!id || room.getDump(id, "").isEditable) {
                                    room.edit = id;
                                    cursorPosition = 0;
                                    Qt.callLater(positionCursorAtEnd);
                                    break;
                                }
                                idx++;
                            }
                        } else if (positionAt(0, cursorRectangle.y + cursorRectangle.height / 2) === 0) {
                            event.accepted = true;
                            positionCursorAtStart();
                        }
                    } else if (event.key == Qt.Key_Down && (event.modifiers == Qt.NoModifier || event.modifiers == Qt.KeypadModifier)) {
                        if (cursorPosition == messageInput.length && room.edit) {
                            event.accepted = true;
                            var idx = room.idToIndex(room.edit) - 1;
                            while (true) {
                                var id = room.indexToId(idx);
                                if (!id || room.getDump(id, "").isEditable) {
                                    room.edit = id;
                                    Qt.callLater(positionCursorAtStart);
                                    break;
                                }
                                idx--;
                            }
                        } else if (positionAt(width, cursorRectangle.y + cursorRectangle.height / 2) === messageInput.length) {
                            event.accepted = true;
                            positionCursorAtEnd();
                        }
                    }
                }
                // Ensure that we get escape key press events first.
                Keys.onShortcutOverride: event => event.accepted = (popup.opened && (event.key === Qt.Key_Escape || event.key === Qt.Key_Tab || event.key === Qt.Key_Enter || event.key === Qt.Key_Space))
                onCursorPositionChanged: {
                    if (!room)
                        return;
                    room.input.updateState(selectionStart, selectionEnd, cursorPosition, text);
                    if (popup.opened && cursorPosition <= completerTriggeredAt)
                        popup.close();
                    if (popup.opened)
                        completer.completer.setSearchString(messageInput.getText(completerTriggeredAt, cursorPosition) + messageInput.preeditText);
                }
                onPreeditTextChanged: {
                    if (popup.opened)
                        completer.completer.setSearchString(messageInput.getText(completerTriggeredAt, cursorPosition) + messageInput.preeditText);
                }
                onSelectionEndChanged: room.input.updateState(selectionStart, selectionEnd, cursorPosition, text)
                onSelectionStartChanged: room.input.updateState(selectionStart, selectionEnd, cursorPosition, text)
                onTextChanged: {
                    const insertedLength = text.length - previousTextLength;
                    if (room)
                        room.input.updateState(selectionStart, selectionEnd, cursorPosition, text);
                    forceActiveFocus();
                    if (cursorPosition > 0)
                        lastChar = text.charAt(cursorPosition - 1);
                    else
                        lastChar = '';
                    const triggerPos = selectionStart - 1;
                    const type = messageInput.completerTypeForTrigger(lastChar, triggerPos);
                    if (type !== "") {
                        messageInput.openCompleter(triggerPos, type);
                    } else if (insertedLength > 1) {
                        messageInput.maybeOpenCompleterForTrailingTokenAfterBulkInsert();
                    }
                    previousTextLength = text.length;
                }

                Connections {
                    function onRoomChanged() {
                        if (TimelineManager.perfUiFlagEnabled("disable_composer"))
                            return;

                        messageInput.clear();
                        if (room)
                            messageInput.append(room.input.text);
                        completer.completerType = "";
                        messageInput.forceActiveFocus();
                        if (room) {
                            const roomId = room.roomId;
                            TimelineManager.markRoomSwitchPhase(roomId, "qml.message_input.room_changed");
                            Qt.callLater(function () {
                                TimelineManager.markRoomSwitchPhase(roomId, "qml.message_input.next_tick");
                            });
                        }
                    }

                    target: timelineView
                }
                Connections {
                    function onCompletionClicked(completion) {
                        messageInput.insertCompletion(completion);
                    }
                    function onCountChanged() {
                        // When the async search settles with zero results and the
                        // typed text already contains a space, close the popup.
                        // This handles the race where the Space key handler fires
                        // before the search has updated (Qt::QueuedConnection),
                        // while still allowing multi-word searches that DO produce
                        // results (e.g. ":bearded woman").
                        if (popup.opened && completer.count <= 0) {
                            var raw = messageInput.getText(messageInput.completerTriggeredAt, messageInput.cursorPosition);
                            if (raw.indexOf(' ') >= 0)
                                popup.close();
                        }
                    }
                    function onDismissed() {
                        completer.completerType = "";
                        popup.close();
                    }

                    target: completer
                }
                Popup {
                    id: popup

                    readonly property real popupMargin: Komai.paddingSmall

                    function clamp(value, minValue, maxValue) {
                        return Math.max(minValue, Math.min(value, maxValue));
                    }
                    function inputBarRectInOverlay() {
                        if (!popup.parent)
                            return Qt.rect(0, 0, inputBar.width, inputBar.height);

                        const topLeft = inputBar.mapToItem(popup.parent, 0, 0);
                        return Qt.rect(topLeft.x, topLeft.y, inputBar.width, inputBar.height);
                    }
                    function anchorRectInOverlay() {
                        const cursorRect = messageInput.positionToRectangle(messageInput.completerTriggeredAt);
                        if (!popup.parent)
                            return cursorRect;

                        const topLeft = messageInput.mapToItem(popup.parent, cursorRect.x, cursorRect.y);
                        return Qt.rect(topLeft.x, topLeft.y, cursorRect.width, cursorRect.height);
                    }
                    function preferredInlineWidth(availableWidth) {
                        const minWidth = completer.emptyStateMinWidth;
                        switch (completer.completerType) {
                        case "roomAliases":
                            return availableWidth;
                        case "user":
                            if (availableWidth <= Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 40)))
                                return availableWidth;
                            return Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 34));
                        case "emoji":
                        case "customEmoji":
                            if (availableWidth <= Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 40)))
                                return availableWidth;
                            return Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 30));
                        case "command":
                            return Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 28));
                        default:
                            return Math.max(minWidth, Math.ceil(Settings.uiFontSizePt * 30));
                        }
                    }

                    background: null
                    padding: 0
                    parent: Overlay.overlay
                    width: {
                        const containerRect = popup.inputBarRectInOverlay();
                        const availableWidth = Math.max(0, containerRect.width - popup.popupMargin * 2);
                        const preferredWidth = popup.preferredInlineWidth(availableWidth);
                        return availableWidth > 0 ? Math.min(preferredWidth, availableWidth) : preferredWidth;
                    }
                    x: {
                        const containerRect = popup.inputBarRectInOverlay();
                        const anchorRect = popup.anchorRectInOverlay();
                        const minX = containerRect.x + popup.popupMargin;
                        const maxX = containerRect.x + containerRect.width - popup.popupMargin - popup.width;
                        return Math.round(popup.clamp(anchorRect.x, minX, Math.max(minX, maxX)));
                    }
                    y: {
                        const anchorRect = popup.anchorRectInOverlay();
                        const popupHeight = Math.max(popup.height, popup.implicitHeight);
                        const overlayHeight = popup.parent ? popup.parent.height : textInput.Window.height;
                        const minY = popup.popupMargin;
                        const maxY = Math.max(minY, overlayHeight - popupHeight - popup.popupMargin);
                        const aboveY = anchorRect.y - popupHeight;
                        const belowY = anchorRect.y + anchorRect.height;
                        const canOpenAbove = aboveY >= minY;
                        const canOpenBelow = belowY + popupHeight <= overlayHeight - popup.popupMargin;

                        if (!canOpenAbove && canOpenBelow)
                            return Math.round(belowY);

                        return Math.round(popup.clamp(aboveY, minY, maxY));
                    }

                    enter: Transition {
                        NumberAnimation {
                            duration: 100
                            from: 0
                            property: "opacity"
                            to: 1
                        }
                    }
                    exit: Transition {
                        NumberAnimation {
                            duration: 100
                            from: 1
                            property: "opacity"
                            to: 0
                        }
                    }

                    onClosed: completer.completerType = ""

                    contentItem: Completer {
                        id: completer

                        rowMargin: 2
                        rowSpacing: 0
                        roomId: room ? room.roomId : ""
                    }
                }
                Connections {
                    function onTextChanged(newText) {
                        messageInput.text = newText;
                        messageInput.cursorPosition = newText.length;
                    }

                    ignoreUnknownSignals: true
                    target: room ? room.input : null
                }
                Connections {
                    function onEditChanged() {
                        messageInput.forceActiveFocus();
                    }
                    function onReplyChanged() {
                        messageInput.forceActiveFocus();
                    }
                    function onThreadChanged() {
                        messageInput.forceActiveFocus();
                    }

                    ignoreUnknownSignals: true
                    target: room
                }
                Connections {
                    function onFocusInput() {
                        messageInput.forceActiveFocus();
                    }

                    target: TimelineManager
                }
                MouseArea {
                    acceptedButtons: Qt.MiddleButton
                    // workaround for wrong cursor shape on some platforms
                    anchors.fill: parent
                    cursorShape: Qt.IBeamCursor

                    onPressed: mouse => mouse.accepted = room.input.tryPasteAttachment(true)
                }
            }
        }
        ComposerToolbarButton {
            id: stickerButton

            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            ToolTip.text: qsTr("Stickers")
            image: ":/icons/icons/ui/sticky-note-solid.svg"
            visible: showAllButtons && Settings.composerExtrasStickersEnabled

            onClicked: stickerPopup.visible ? stickerPopup.close() : stickerPopup.show(stickerButton, room.roomId, function (row) {
                    room.input.sticker(row);
                    TimelineManager.focusMessageInput();
                }, inputBar)

            StickerPicker {
                id: stickerPopup

                emoji: false
            }
        }
        ComposerToolbarButton {
            id: emojiButton

            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            ToolTip.text: qsTr("Emoji")
            image: ":/icons/icons/ui/smile.svg"
            visible: inputBar.composerEnabled

            onClicked: emojiPopup.visible ? emojiPopup.close() : emojiPopup.show(emojiButton, room.roomId, function (plaintext, markdown) {
                    messageInput.insert(messageInput.cursorPosition, markdown);
                    TimelineManager.focusMessageInput();
                }, inputBar)

            StickerPicker {
                id: emojiPopup

                emoji: true
            }
        }
        ComposerToolbarButton {
            id: sendButton

            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            Layout.rightMargin: 8
            ToolTip.text: qsTr("Send")
            buttonTextColor: inputBar.hasSendableContent ? palette.highlight : palette.buttonText
            image: ":/icons/icons/ui/send.svg"

            SequentialAnimation {
                id: shakeAnimation

                NumberAnimation { target: sendButton; property: "rotation"; to: -15; duration: 50 }
                NumberAnimation { target: sendButton; property: "rotation"; to: 15; duration: 80 }
                NumberAnimation { target: sendButton; property: "rotation"; to: -10; duration: 70 }
                NumberAnimation { target: sendButton; property: "rotation"; to: 10; duration: 60 }
                NumberAnimation { target: sendButton; property: "rotation"; to: 0; duration: 50 }
            }

            Timer {
                id: shakeTimer

                interval: 500
                repeat: false
                onTriggered: {
                    if (inputBar.hasSendableContent && Settings.uiMotionAnimationsEnabled)
                        shakeAnimation.start();
                }
            }

            Connections {
                target: messageInput
                function onTextChanged() {
                    if (messageInput.length > 0 && Settings.uiMotionAnimationsEnabled)
                        shakeTimer.restart();
                    else
                        shakeTimer.stop();
                }
            }

            Connections {
                target: room ? room.input : null
                ignoreUnknownSignals: true
                function onUploadsChanged() {
                    if (inputBar.hasUploads && Settings.uiMotionAnimationsEnabled)
                        shakeTimer.restart();
                }
            }

            onClicked: {
                room.input.send();
            }
        }
    }
    Label {
        anchors.centerIn: parent
        color: Komai.theme.warning
        text: qsTr("You don't have permission to send messages in this room")
        visible: room ? (!room.permissions.canSend(MtxEvent.TextMessage)) : false
    }
}
