// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: support

    required property var rootItem
    required property var composerPane
    required property var roomSupport
    required property var timelineList

    function openMatrixMessageContextMenu(messageModel, roomModel, copyText) {
        if (!messageModel || !roomModel || !messageModel.eventId)
            return;

        roomSupport.messageContextMenu.show(messageModel.eventId,
                                            messageModel.threadId || "",
                                            messageModel.type,
                                            !!messageModel.isSender,
                                            !!messageModel.isEncrypted,
                                            !!messageModel.isEditable,
                                            !!messageModel.isStateEvent,
                                            "",
                                            copyText || "",
                                            null,
                                            messageModel,
                                            roomModel);
    }

    function jumpToLoadedMatrixEvent(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0 || !TimelineManager.matrixTimelineModel || !timelineList)
            return false;

        const row = TimelineManager.matrixTimelineModel.rowForEventId(trimmedEventId);
        if (row < 0)
            return false;

        timelineList.positionViewAtIndex(row, ListView.Center);
        return true;
    }

    function focusTextInput() {
        return composerPane && composerPane.composerInput
            ? composerPane.composerInput.focusTextInput()
            : false;
    }

    function destroyOnClose(dialog) {
        return roomSupport.destroyOnClose(dialog);
    }

    function scheduleComposerAutoFocus() {
        if (!rootItem.pendingComposerAutoFocus)
            return;

        Qt.callLater(function () {
            if (!rootItem.pendingComposerAutoFocus
                    || !rootItem.visible
                    || rootItem.perfDisableComposer
                    || rootItem.walkModeActive
                    || rootItem.hasOpenOverlayDialog) {
                return;
            }

            if (support.focusTextInput())
                rootItem.pendingComposerAutoFocus = false;
        });
    }

    function shouldRouteTextKeyToComposer(event) {
        if (!event
                || rootItem.walkModeActive
                || rootItem.headerSearchHasFocus
                || rootItem.hasOpenOverlayDialog) {
            return false;
        }

        const text = String(event.text || "");
        if (text.length === 0)
            return false;

        if (event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter
                || event.key === Qt.Key_Tab
                || event.key === Qt.Key_Backtab) {
            return false;
        }

        const modifiers = Number(event.modifiers);
        return (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function handleComposerTextKey(event) {
        if (!support.shouldRouteTextKeyToComposer(event))
            return false;

        if (!support.appendText(event.text))
            return false;

        rootItem.pendingComposerAutoFocus = false;
        event.accepted = true;
        return true;
    }

    function appendText(text) {
        return composerPane.composerInput ? composerPane.composerInput.appendText(text) : false;
    }

    function trySendMessage() {
        if (rootItem.hasPendingAttachments)
            return TimelineManager.sendActiveMatrixAttachments();

        const body = composerPane.composerInput.text;
        const ok = rootItem.editing
            ? TimelineManager.sendActiveMatrixEditMessage(body)
            : TimelineManager.sendActiveMatrixTextMessage(body);
        if (!ok)
            return false;

        if (!rootItem.editing) {
            composerPane.composerInput.replaceText("");
            roomSupport.composerInputController.setText("");
        }

        support.focusTextInput();
        return true;
    }

    function beginEdit(eventId, body, messageKind) {
        if (!eventId || !body)
            return false;

        if (!rootItem.editing) {
            rootItem.draftBeforeEdit = composerPane.composerInput.text;
            rootItem.restoringEditDraft = true;
        }

        if (!TimelineManager.queueActiveMatrixEdit(String(eventId),
                                                   String(body),
                                                   String(messageKind || "message"))) {
            if (rootItem.restoringEditDraft) {
                rootItem.draftBeforeEdit = "";
                rootItem.restoringEditDraft = false;
            }
            return false;
        }

        roomSupport.composerInputController.setText(String(body));
        support.focusTextInput();
        return true;
    }

    function openRemoveMessageDialog(eventId) {
        return roomSupport.openRemoveMessageDialog(eventId);
    }

    function openRawMessageDialog(eventId) {
        return roomSupport.openRawMessageDialog(eventId);
    }

    function openReadReceiptsDialog(eventId) {
        return roomSupport.openReadReceiptsDialog(eventId);
    }

    function openMatrixForwardDialog(eventId) {
        return roomSupport.openMatrixForwardDialog(eventId);
    }

    function openForwardDialog(eventId) {
        return roomSupport.openMatrixForwardDialog(eventId);
    }

    function openReportMessageDialog(eventId) {
        return roomSupport.openReportMessageDialog(eventId);
    }

    function openMessageActionsDialog(eventId,
                                      threadId,
                                      eventType,
                                      isSender,
                                      isEncrypted,
                                      isEditable,
                                      link,
                                      text,
                                      messageModelOverride,
                                      roomModelOverride) {
        return roomSupport.openMessageActionsDialog(eventId,
                                                    threadId,
                                                    eventType,
                                                    isSender,
                                                    isEncrypted,
                                                    isEditable,
                                                    link,
                                                    text,
                                                    messageModelOverride,
                                                    roomModelOverride);
    }

    property var timelineConnections: Connections {
        target: TimelineManager

        function onMatrixTimelineStateChanged() {
            if (rootItem.pendingComposerAutoFocus)
                support.scheduleComposerAutoFocus();

            rootItem.ensureInitialBottomPin();
            if (rootItem.deferredInitialBufferTopUpPending)
                rootItem.scheduleDeferredInitialTimelineBufferCheck();
            else
                rootItem.scheduleInitialTimelineBufferCheck();

            if (!rootItem.restoringEditDraft || rootItem.activeEditEventId.length > 0)
                return;

            roomSupport.composerInputController.setText(rootItem.draftBeforeEdit);
            rootItem.draftBeforeEdit = "";
            rootItem.restoringEditDraft = false;
            support.focusTextInput();
        }

        function onFocusInput() {
            support.focusTextInput();
        }
    }
}
