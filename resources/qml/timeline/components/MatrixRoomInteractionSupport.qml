// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: support

    required property var rootItem
    required property var composerPane
    required property var dialogSupport
    required property var composerInputController
    required property var timelineList

    function openMatrixMessageContextMenu(messageModel, roomModel, copyText) {
        if (!messageModel || !roomModel || !messageModel.eventId)
            return;

        support.dialogSupport.messageContextMenu.show(messageModel.eventId,
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

        let targetEventId = trimmedEventId;
        let row = TimelineManager.matrixTimelineModel.rowForEventId(targetEventId);
        if (row < 0)
            return false;

        if (!rootItem.isSelectableMatrixTimelineRow(row)) {
            targetEventId = String(rootItem.selectableEventIdNearMatrixRow(row) || "");
            if (targetEventId.length === 0)
                return false;

            row = TimelineManager.matrixTimelineModel.rowForEventId(targetEventId);
            if (row < 0)
                return false;
        }

        rootItem.highlightedEventId = "";
        timelineList.positionViewAtIndex(row, ListView.Center);
        rootItem.highlightedEventId = targetEventId;
        return true;
    }

    function focusTextInput() {
        return composerPane && composerPane.composerInput
            ? composerPane.composerInput.focusTextInput()
            : false;
    }

    function destroyOnClose(dialog) {
        return support.dialogSupport.destroyOnClose(dialog);
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

    function setComposerText(text) {
        const normalized = String(text || "");

        if (composerPane.composerInput)
            composerPane.composerInput.replaceText(normalized);

        if (composerInputController && typeof composerInputController.setText === "function")
            composerInputController.setText(normalized);
    }

    function trySendMessage() {
        if (rootItem.hasPendingAttachments)
            return TimelineManager.sendActiveMatrixAttachments();

        const body = composerPane.composerInput.text;
        if (!rootItem.editing) {
            const inspection = TimelineManager.inspectActiveMatrixSlashCommand(body);
            const submitAction = String((inspection && inspection.submitAction) || "none");

            if (submitAction === "preserveComposer")
                return false;

            if (submitAction === "executeCommand") {
                const commandOk = TimelineManager.executeActiveMatrixSlashCommand(body);
                if (!commandOk)
                    return false;

                support.setComposerText("");
                support.focusTextInput();
                return true;
            }
        }

        const ok = rootItem.editing
            ? TimelineManager.sendActiveMatrixEditMessage(body)
            : TimelineManager.sendActiveMatrixTextMessage(body);
        if (!ok)
            return false;

        if (!rootItem.editing)
            support.setComposerText("");

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

        support.setComposerText(body);
        support.focusTextInput();
        return true;
    }

    function openRemoveMessageDialog(eventId) {
        return support.dialogSupport.openRemoveMessageDialog(eventId);
    }

    function openRawMessageDialog(eventId) {
        return support.dialogSupport.openRawMessageDialog(eventId);
    }

    function openReadReceiptsDialog(eventId) {
        return support.dialogSupport.openReadReceiptsDialog(eventId);
    }

    function openMatrixForwardDialog(eventId) {
        return support.dialogSupport.openMatrixForwardDialog(eventId);
    }

    function openForwardDialog(eventId) {
        return support.dialogSupport.openMatrixForwardDialog(eventId);
    }

    function openReportMessageDialog(eventId) {
        return support.dialogSupport.openReportMessageDialog(eventId);
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
        return support.dialogSupport.openMessageActionsDialog(eventId,
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

    function resolvePendingMatrixEventJump() {
        const pendingEventId = String(TimelineManager.matrixTimelinePendingJumpEventId || "").trim();
        if (pendingEventId.length === 0)
            return false;

        if (!TimelineManager.resolveActiveMatrixPendingJump())
            return false;

        if (!support.jumpToLoadedMatrixEvent(pendingEventId))
            return false;

        TimelineManager.clearActiveMatrixPendingJump(pendingEventId);
        return true;
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
                return support.resolvePendingMatrixEventJump();

            support.setComposerText(rootItem.draftBeforeEdit);
            rootItem.draftBeforeEdit = "";
            rootItem.restoringEditDraft = false;
            support.focusTextInput();
            support.resolvePendingMatrixEventJump();
        }

        function onFocusInput() {
            support.focusTextInput();
        }
    }
}
