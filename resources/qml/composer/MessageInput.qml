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
    required property var selectionModeRoot
    property var inputController: room && room.input ? room.input : null
    property bool allowCalls: true
    property bool allowStickers: true
    property bool allowCommandCompleter: true
    property bool attachmentsEnabled: true
    property bool showAllButtons: width > 450 || (messageInput.length == 0 && !messageInput.inputMethodComposing)
    property bool walkModeActive: false
    readonly property string text: messageInput.text
    readonly property bool textInputActiveFocus: messageInput.activeFocus
    readonly property bool hasUploads: !!(inputController && inputController.uploads && inputController.uploads.length > 0)
    readonly property bool composerEnabled: !hasUploads
    readonly property bool hasSendableContent: messageInput.length > 0 || hasUploads
    readonly property int minimumBarHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property bool composerExpanded: textInput.targetTextAreaHeight > textInput.singleLineHeight
    readonly property bool commandPickerVisible: popup.opened && completer.completerType === "command"
    readonly property int permissionsRevision: room && room.permissions ? room.permissions.revision : 0
    readonly property bool canSendCurrentRoom: {
        const _ = permissionsRevision;
        return room ? room.permissions.canSend(room.isEncrypted ? MtxEvent.Encrypted : MtxEvent.TextMessage) : false;
    }
    readonly property bool canSendTextMessages: {
        const _ = permissionsRevision;
        return room ? room.permissions.canSend(MtxEvent.TextMessage) : false;
    }
    signal composerInteractionRequested()

    function focusTextInput() {
        if (walkModeActive) {
            composerInteractionRequested();
            return false;
        }

        messageInput.forceActiveFocus();
        return !!messageInput.activeFocus;
    }

    function focusTextInputIfAllowed() {
        if (walkModeActive)
            return false;

        messageInput.forceActiveFocus();
        return !!messageInput.activeFocus;
    }

    function appendText(text) {
        if (!text)
            return false;

        messageInput.forceActiveFocus();
        messageInput.insert(messageInput.cursorPosition, text);
        return true;
    }

    function replaceText(text) {
        const value = String(text || "");
        messageInput.text = value;
        messageInput.cursorPosition = value.length;
        return true;
    }

    function eventMatchesLatinKey(event, latinKey) {
        if (!event)
            return false;

        return LayoutAgnosticKeys.matchesLatinKey(latinKey,
                                                  event.key,
                                                  event.nativeScanCode);
    }

    function isBackwardTabEvent(event) {
        if (!event)
            return false;

        const modifiers = Number(event.modifiers);
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) !== 0)
            return false;

        return event.key === Qt.Key_Backtab
            || (event.key === Qt.Key_Tab && (modifiers & Qt.ShiftModifier) !== 0);
    }

    function isForwardTabEvent(event) {
        if (!event)
            return false;

        return event.key === Qt.Key_Tab && event.modifiers === Qt.NoModifier;
    }

    function isComposerTabEvent(event) {
        return isForwardTabEvent(event) || isBackwardTabEvent(event);
    }

    function requestSelectionModeEntry() {
        if (!selectionModeRoot || typeof selectionModeRoot.enterWalkModeFromBottomMostVisible !== "function")
            return false;

        return selectionModeRoot.enterWalkModeFromBottomMostVisible();
    }

    function requestSelectionModeOlderChunk() {
        if (!selectionModeRoot || typeof selectionModeRoot.enterWalkModeAndMoveTowardOlderEventsByChunk !== "function")
            return false;

        return selectionModeRoot.enterWalkModeAndMoveTowardOlderEventsByChunk();
    }

    function roomHeaderBacktabTarget() {
        if (!selectionModeRoot || typeof selectionModeRoot.lastRoomHeaderActionButtonTarget !== "function")
            return null;

        return selectionModeRoot.lastRoomHeaderActionButtonTarget();
    }

    function composerBacktabTarget() {
        if (attachButton.visible && attachButton.enabled)
            return attachButton;
        if (callButton.visible && callButton.enabled)
            return callButton;
        return roomHeaderBacktabTarget();
    }

    function composerTabTarget() {
        if (stickerButton.visible && stickerButton.enabled)
            return stickerButton;
        if (emojiButton.visible && emojiButton.enabled)
            return emojiButton;
        if (sendButton.visible && sendButton.enabled)
            return sendButton;
        return null;
    }

    function focusComposerBacktabTarget() {
        const target = composerBacktabTarget();
        if (!target)
            return false;

        target.forceActiveFocus(Qt.TabFocusReason);
        return true;
    }

    function focusComposerTabTarget() {
        const target = composerTabTarget();
        if (!target)
            return false;

        target.forceActiveFocus(Qt.TabFocusReason);
        return true;
    }

    Layout.fillWidth: true
    implicitHeight: Math.max(minimumBarHeight, row.implicitHeight)
    Layout.minimumHeight: minimumBarHeight
    Layout.preferredHeight: implicitHeight
    color: inputBar.hasUploads || (room && !inputBar.canSendTextMessages) ? palette.alternateBase : palette.window

    RowLayout {
        id: row

        anchors.fill: parent
        spacing: 0
        visible: inputBar.canSendCurrentRoom

        ComposerCallButton {
            id: callButton

            Layout.alignment: inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter
            Layout.leftMargin: Komai.paddingMedium
            KeyNavigation.backtab: inputBar.roomHeaderBacktabTarget()
            KeyNavigation.tab: attachButton.visible ? attachButton : messageInput
            room: inputBar.room
            timelineRoot: inputBar.timelineRoot
            showAllButtons: inputBar.showAllButtons && inputBar.allowCalls
        }
        ComposerAttachButton {
            id: attachButton

            Layout.alignment: inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter
            Layout.leftMargin: callButton.visible ? 0 : Komai.paddingMedium
            enabled: inputBar.attachmentsEnabled
            KeyNavigation.backtab: callButton.visible ? callButton : inputBar.roomHeaderBacktabTarget()
            KeyNavigation.tab: messageInput
            room: inputBar.room
            showAllButtons: inputBar.showAllButtons
        }
        ScrollView {
            id: textInput

            readonly property int singleLineHeight: Math.ceil(fontMetrics.lineSpacing + messageInput.topPadding + messageInput.bottomPadding)
            readonly property int targetTextAreaHeight: Math.max(singleLineHeight,
                                                                 Math.ceil(messageInput.contentHeight + messageInput.topPadding + messageInput.bottomPadding))
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.maximumHeight: Window.height / 4
            Layout.minimumHeight: visible ? targetTextAreaHeight : 0
            Layout.preferredHeight: visible ? targetTextAreaHeight : 0
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            contentWidth: availableWidth
            implicitHeight: targetTextAreaHeight
            padding: 0

            TextArea {
                id: messageInput

                property int completerTriggeredAt: 0
                property string lastChar
                property int previousTextLength: 0

                function currentCompleterSearchString() {
                    if (completer.completerType === "command" && inputBar.inputController) {
                        const prefix = messageInput.getText(0, cursorPosition) + messageInput.preeditText;
                        return inputBar.inputController.commandCompletionSearchString(prefix,
                                                                                      cursorPosition + messageInput.preeditText.length);
                    }

                    return messageInput.getText(completerTriggeredAt, cursorPosition) + messageInput.preeditText;
                }
                function hasNestedCompleterTrigger(searchString) {
                    const suffix = String(searchString || "").slice(1);
                    if (suffix.length === 0)
                        return false;

                    switch (completer.completerType) {
                    case "user":
                        return suffix.indexOf('@') >= 0 || suffix.indexOf('＠') >= 0;
                    case "emoji":
                        return suffix.indexOf(':') >= 0 || suffix.indexOf('：') >= 0;
                    case "roomAliases":
                        return suffix.indexOf('#') >= 0 || suffix.indexOf('＃') >= 0;
                    case "customEmoji":
                        return suffix.indexOf('~') >= 0 || suffix.indexOf('～') >= 0;
                    default:
                        return false;
                    }
                }
                function refreshCompleterSearchString() {
                    if (!popup.opened || !completer.completer)
                        return;

                    const searchString = messageInput.currentCompleterSearchString();
                    if (messageInput.hasNestedCompleterTrigger(searchString)) {
                        completer.completerType = "";
                        popup.close();
                        return;
                    }

                    completer.completer.setSearchString(searchString);
                }
                function insertCompletion(completion) {
                    if (completer.completerType === "command" && inputBar.inputController) {
                        const updatedText = inputBar.inputController.applyCommandCompletion(messageInput.text,
                                                                                            cursorPosition,
                                                                                            completion);
                        const updatedCursorPosition = inputBar.inputController.commandCompletionCursorPosition(messageInput.text,
                                                                                                               cursorPosition,
                                                                                                               completion);
                        messageInput.text = updatedText;
                        messageInput.cursorPosition = updatedCursorPosition;
                        return;
                    }

                    let replaceEnd = cursorPosition;
                    messageInput.remove(completerTriggeredAt, replaceEnd);
                    messageInput.insert(completerTriggeredAt, completion);
                    messageInput.cursorPosition = completerTriggeredAt + completion.length;
                    let userid = completer.currentUserid();
                    if (userid && inputBar.inputController)
                        inputBar.inputController.addMention(userid, completion);
                }
                function openCompleter(pos, type) {
                    completerTriggeredAt = pos;
                    completer.completerType = type;
                    if (!popup.opened)
                        popup.open();
                    messageInput.refreshCompleterSearchString();
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
                    if (inputBar.allowCommandCompleter
                            && (trigger === '/' || trigger === '／')
                            && tokenStart === 0)
                        return "command";
                    return "";
                }
                function isCursorOnTopLine() {
                    return currentVisualLineStartPosition() === 0;
                }
                function currentVisualLineStartPosition() {
                    return positionAt(0, cursorRectangle.y + cursorRectangle.height / 2);
                }
                function currentVisualLineEndPosition() {
                    return positionAt(width, cursorRectangle.y + cursorRectangle.height / 2);
                }
                function isCursorAtSelectionModeBoundary() {
                    if (text.length === 0 && cursorPosition === 0)
                        return true;

                    if (!messageInput.isCursorOnTopLine())
                        return false;

                    return cursorPosition === currentVisualLineStartPosition();
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
                bottomPadding: 6
                color: palette.text
                enabled: inputBar.composerEnabled
                focus: true
                leftPadding: inputBar.showAllButtons ? Komai.paddingSmall : 8
                padding: 0
                font.pointSize: Settings.uiFontSizePt
                placeholderText: inputBar.hasUploads ? "" : qsTr("Write a message, or press Up to select messages.")
                placeholderTextColor: palette.buttonText
                selectByMouse: true
                topPadding: 6
                verticalAlignment: TextEdit.AlignVCenter
                implicitHeight: textInput.targetTextAreaHeight
                width: textInput.width
                wrapMode: TextEdit.Wrap

                Keys.onPressed: event => {
                    if (event.modifiers === (Qt.ControlModifier | Qt.ShiftModifier) && event.key === Qt.Key_V) {
                        // Ctrl+Shift+V: paste as plain text (Qt doesn't handle this natively,
                        // and the unhandled key event produces a control character / tofu square)
                        var clipText = inputBar.inputController ? inputBar.inputController.clipboardText() : "";
                        if (clipText)
                            messageInput.insert(messageInput.cursorPosition, clipText);
                        event.accepted = true;
                    } else if (event.matches(StandardKey.Paste)) {
                        event.accepted = inputBar.inputController
                            ? inputBar.inputController.tryPasteAttachment(false)
                            : false;
                    } else if (event.key == Qt.Key_Space) {
                        // close popup if user enters space after colon
                        if (cursorPosition == completerTriggeredAt + 1)
                            popup.close();
                        if (popup.opened && completer.count <= 0)
                            popup.close();
                    } else if (event.modifiers == Qt.ControlModifier && inputBar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.U)) {
                        event.accepted = inputBar.requestSelectionModeOlderChunk();
                    } else if (event.modifiers == Qt.ControlModifier && event.key == Qt.Key_P) {
                        messageInput.text = inputBar.inputController
                            ? inputBar.inputController.previousText()
                            : messageInput.text;
                    } else if (event.modifiers == Qt.ControlModifier && event.key == Qt.Key_N) {
                        messageInput.text = inputBar.inputController
                            ? inputBar.inputController.nextText()
                            : messageInput.text;
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
                                    if (inputBar.inputController)
                                        inputBar.inputController.addMention(userid, currentCompletion);
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
                            if (inputBar.inputController)
                                inputBar.inputController.send();
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
                    } else if (inputBar.isComposerTabEvent(event)) {
                        if (!popup.opened && messageInput.length === 0 && !messageInput.inputMethodComposing) {
                            event.accepted = true;
                            if (inputBar.isBackwardTabEvent(event))
                                inputBar.focusComposerBacktabTarget();
                            else
                                inputBar.focusComposerTabTarget();
                            return;
                        }

                        event.accepted = true;
                        if (popup.opened) {
                            if (inputBar.isBackwardTabEvent(event))
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
                    } else if (event.key == Qt.Key_Down && popup.opened) {
                        event.accepted = true;
                        completer.down();
                    } else if (event.key == Qt.Key_Up && (event.modifiers == Qt.NoModifier || event.modifiers == Qt.KeypadModifier)) {
                        if (messageInput.isCursorAtSelectionModeBoundary()) {
                            event.accepted = inputBar.requestSelectionModeEntry();
                        } else if (messageInput.isCursorOnTopLine()) {
                            event.accepted = true;
                            cursorPosition = messageInput.currentVisualLineStartPosition();
                        }
                    } else if (event.key == Qt.Key_Down && (event.modifiers == Qt.NoModifier || event.modifiers == Qt.KeypadModifier)) {
                        const lineEnd = messageInput.currentVisualLineEndPosition();
                        if (cursorPosition !== lineEnd) {
                            event.accepted = true;
                            cursorPosition = lineEnd;
                        }
                    }
                }
                // Ensure that we get escape key press events first.
                Keys.onShortcutOverride: event => {
                    if (inputBar.isComposerTabEvent(event)) {
                        event.accepted = true;
                        return;
                    }

                    event.accepted = popup.opened
                        && (event.key === Qt.Key_Escape
                            || event.key === Qt.Key_Tab
                            || event.key === Qt.Key_Backtab
                            || event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Space);
                }
                onCursorPositionChanged: {
                    if (!inputBar.inputController)
                        return;
                    inputBar.inputController.updateState(selectionStart, selectionEnd, cursorPosition, text);
                    if (popup.opened && cursorPosition <= completerTriggeredAt)
                        popup.close();
                    messageInput.refreshCompleterSearchString();
                }
                onPreeditTextChanged: {
                    messageInput.refreshCompleterSearchString();
                }
                onSelectionEndChanged: {
                    if (inputBar.inputController)
                        inputBar.inputController.updateState(selectionStart, selectionEnd, cursorPosition, text);
                }
                onSelectionStartChanged: {
                    if (inputBar.inputController)
                        inputBar.inputController.updateState(selectionStart, selectionEnd, cursorPosition, text);
                }
                onTextChanged: {
                    const insertedLength = text.length - previousTextLength;
                    if (inputBar.inputController)
                        inputBar.inputController.updateState(selectionStart, selectionEnd, cursorPosition, text);
                    inputBar.focusTextInputIfAllowed();
                    if (cursorPosition > 0)
                        lastChar = text.charAt(cursorPosition - 1);
                    else
                        lastChar = '';
                    const triggerPos = selectionStart - 1;
                    const type = messageInput.completerTypeForTrigger(lastChar, triggerPos);
                    if (type !== "") {
                        const charBefore = triggerPos > 0 ? text.charAt(triggerPos - 1) : '';
                        const atWordBoundary = triggerPos === 0
                            || charBefore === ' '
                            || charBefore === '\t'
                            || charBefore === '\n'
                            || charBefore === '\r'
                            || charBefore === '\u3000';
                        if (type === "command" || atWordBoundary)
                            messageInput.openCompleter(triggerPos, type);
                    } else if (insertedLength > 1) {
                        messageInput.maybeOpenCompleterForTrailingTokenAfterBulkInsert();
                    }
                    previousTextLength = text.length;
                }

                Connections {
                    function onRoomPreviewChanged() {
                        if (TimelineManager.perfUiFlagEnabled("disable_composer"))
                            return;

                        messageInput.clear();
                        if (inputBar.inputController && inputBar.inputController.text)
                            messageInput.append(inputBar.inputController.text);
                        completer.completerType = "";
                        inputBar.focusTextInputIfAllowed();
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
                        if (popup.opened && completer.completerType !== "command" && completer.count <= 0) {
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
                    readonly property bool composerIsTall: textInput.height > textInput.singleLineHeight * 2.5

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
                            return availableWidth;
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
                        const anchorRect = popup.composerIsTall ? popup.anchorRectInOverlay() : popup.inputBarRectInOverlay();
                        const popupHeight = Math.max(popup.height, popup.implicitHeight);
                        const overlayHeight = popup.parent ? popup.parent.height : textInput.Window.height;
                        const minY = popup.popupMargin;
                        const maxY = Math.max(minY, overlayHeight - popupHeight - popup.popupMargin);
                        const gap = popup.composerIsTall ? 0 : popup.popupMargin;
                        const aboveY = anchorRect.y - popupHeight - gap;
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

                        commandValidationMessage: inputBar.inputController ? inputBar.inputController.commandValidationMessage : ""
                        commandValidationState: inputBar.inputController ? inputBar.inputController.commandValidationState : "none"
                        rowMargin: 2
                        rowSpacing: 0
                        roomId: room ? room.roomId : ""
                    }
                }
                Connections {
                    function onTextChanged(newText) {
                        const updatedText = newText !== undefined
                            ? String(newText)
                            : String((inputBar.inputController && inputBar.inputController.text) || "");
                        if (messageInput.text === updatedText)
                            return;
                        messageInput.text = updatedText;
                        messageInput.cursorPosition = updatedText.length;
                    }

                    ignoreUnknownSignals: true
                    target: inputBar.inputController
                }
                Connections {
                    function onEditChanged() {
                        inputBar.focusTextInput();
                    }
                    function onReplyChanged() {
                        inputBar.focusTextInput();
                    }
                    function onThreadChanged() {
                        inputBar.focusTextInput();
                    }

                    ignoreUnknownSignals: true
                    target: room
                }
                Connections {
                    function onFocusInput() {
                        inputBar.focusTextInput();
                    }

                    target: TimelineManager
                }
                MouseArea {
                    acceptedButtons: Qt.MiddleButton
                    // workaround for wrong cursor shape on some platforms
                    anchors.fill: parent
                    cursorShape: Qt.IBeamCursor

                    onPressed: mouse => mouse.accepted = inputBar.inputController
                        ? inputBar.inputController.tryPasteAttachment(true)
                        : false
                }
            }
        }
        ComposerToolbarButton {
            id: stickerButton

            Layout.alignment: Qt.AlignRight | (inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter)
            KeyNavigation.backtab: messageInput
            KeyNavigation.tab: emojiButton.visible ? emojiButton : sendButton
            toolTipText: qsTr("Stickers")
            image: ":/icons/icons/ui/sticky-note-solid.svg"
            visible: showAllButtons && inputBar.allowStickers && Settings.composerExtrasStickersEnabled

            onClicked: {
                if (!inputBar.inputController || typeof inputBar.inputController.sticker !== "function")
                    return;

                stickerPopup.visible ? stickerPopup.close() : stickerPopup.show(stickerButton, room.roomId, function (row) {
                        inputBar.inputController.sticker(row);
                        TimelineManager.focusMessageInput();
                    }, inputBar);
            }

            StickerPicker {
                id: stickerPopup

                emoji: false
            }
        }
        ComposerToolbarButton {
            id: emojiButton

            Layout.alignment: Qt.AlignRight | (inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter)
            KeyNavigation.backtab: stickerButton.visible ? stickerButton : messageInput
            KeyNavigation.tab: sendButton
            toolTipText: qsTr("Emoji")
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

            Layout.alignment: Qt.AlignRight | (inputBar.composerExpanded ? Qt.AlignBottom : Qt.AlignVCenter)
            Layout.rightMargin: Komai.paddingMedium
            KeyNavigation.backtab: emojiButton.visible ? emojiButton : (stickerButton.visible ? stickerButton : messageInput)
            toolTipText: qsTr("Send")
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
                target: inputBar.inputController
                ignoreUnknownSignals: true
                function onUploadsChanged() {
                    if (inputBar.hasUploads && Settings.uiMotionAnimationsEnabled)
                        shakeTimer.restart();
                }
            }

            onClicked: {
                if (inputBar.inputController)
                    inputBar.inputController.send();
            }
        }
    }
    Label {
        anchors.centerIn: parent
        color: palette.buttonText
        text: qsTr("You don't have permission to send messages in this room")
        visible: room ? (!inputBar.canSendTextMessages) : false
    }
    Label {
        anchors.centerIn: parent
        color: palette.buttonText
        text: qsTr("Attach more files or send the upload")
        visible: inputBar.hasUploads
    }
}
