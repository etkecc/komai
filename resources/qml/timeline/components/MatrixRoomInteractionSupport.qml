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
        if (!messageModel || !roomModel)
            return;

        // Accept local echoes (no server event_id yet, only a transactionId) —
        // we still want users to reach "Cancel send" on them.
        const hasIdentity = !!messageModel.eventId || !!messageModel.transactionId;
        if (!hasIdentity)
            return;

        support.dialogSupport.messageContextMenu.show(messageModel.eventId || "",
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
                                                      roomModel,
                                                      messageModel.transactionId || "");
    }

    // One-shot re-center shortly after a jump: delegates created by
    // positionViewAtIndex can change height once their async content
    // (images, avatars) sizes itself, sliding the target partially out
    // of view. Model resets re-anchor via jumpAnchorEventId, but a quiet
    // timeline gets no reset to correct the drift.
    property string _jumpSettleTarget: ""
    property var _jumpSettleTimer: Timer {
        interval: 450
        onTriggered: {
            const target = support._jumpSettleTarget;
            support._jumpSettleTarget = "";
            if (target.length === 0 || !support.timelineList
                    || support.rootItem.jumpAnchorEventId !== target) {
                return;
            }
            const listModel = support.timelineList.model;
            const row = (listModel && typeof listModel.rowForEventId === "function")
                ? listModel.rowForEventId(target)
                : -1;
            if (row < 0 || row >= support.timelineList.count)
                return;
            support.timelineList.positionViewAtIndex(row, ListView.Center);
            support.timelineList.returnToBounds();
        }
    }

    function jumpToLoadedMatrixEvent(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        // Selectability is decided on `activeTimelineModel` (the model the
        // selection logic walks)...
        const selModel = rootItem.activeTimelineModel;
        if (trimmedEventId.length === 0 || !selModel || !timelineList)
            return false;

        let targetEventId = trimmedEventId;
        let selRow = selModel.rowForEventId(targetEventId);
        if (selRow < 0)
            return false;

        if (!rootItem.isSelectableMatrixTimelineRow(selRow)) {
            targetEventId = String(rootItem.selectableEventIdNearMatrixRow(selRow) || "");
            if (targetEventId.length === 0)
                return false;
        }

        // ...but the row handed to positionViewAtIndex must come from the
        // ListView's *own* model, which may be a filter proxy with a
        // different row space (search / collapse-thread-replies).
        const listModel = timelineList.model;
        const listRow = (listModel && typeof listModel.rowForEventId === "function")
            ? listModel.rowForEventId(targetEventId)
            : -1;
        if (listRow < 0)
            return false;

        rootItem.highlightedEventId = "";
        // Jumping away from the live edge must release the bottom pin,
        // or the pin machinery (and the pinned-restore path of the next
        // model reset) snaps the view straight back to the bottom.
        // Mirrors walk-mode's unpin-on-move.
        timelineList.keepPinnedToBottom = false;
        timelineList.userUnpinned = true;
        rootItem.jumpAnchorEventId = targetEventId;
        timelineList.positionViewAtIndex(listRow, ListView.Center);
        support._jumpSettleTarget = targetEventId;
        support._jumpSettleTimer.restart();
        // Set the highlight a frame later: on a cold jump the target's
        // delegate is only instantiated after positionViewAtIndex, and a
        // scrolledToThis state that is already active when the delegate
        // is created skips the highlight-flash transition entirely.
        const highlightTarget = targetEventId;
        Qt.callLater(function () {
            if (rootItem.highlightedEventId.length === 0)
                rootItem.highlightedEventId = highlightTarget;
        });
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

    property var _autoFocusRetryTimer: Timer {
        interval: 50
        onTriggered: support.scheduleComposerAutoFocus()
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

            if (support.focusTextInput()) {
                rootItem.pendingComposerAutoFocus = false;
                rootItem._composerAutoFocusRetries = 0;
            } else if (rootItem._composerAutoFocusRetries < 10) {
                rootItem._composerAutoFocusRetries++;
                support._autoFocusRetryTimer.restart();
            }
        });
    }

    function composerHasTextInputFocus() {
        return !!(composerPane
            && composerPane.composerInput
            && composerPane.composerInput.textInputActiveFocus);
    }

    function hasInsertableComposerText(text) {
        const value = String(text || "");
        return value.length > 0 && !/[\u0000-\u001F\u007F-\u009F]/.test(value);
    }

    function messageActionsControl() {
        return dialogSupport && dialogSupport.messageActionsHost
            ? dialogSupport.messageActionsHost.control
            : null;
    }

    function dismissPinnedMessageActions() {
        const control = support.messageActionsControl();
        if (!control || !control.pinned || typeof control.dismiss !== "function")
            return false;

        control.dismiss();
        if (rootItem.walkModeActive)
            rootItem.focusTimelineSelection();
        else if (!rootItem.headerSearchHasFocus)
            Qt.callLater(function () {
                support.focusTextInput();
            });

        return true;
    }

    function canHandleEscape() {
        if (rootItem.hasOpenOverlayDialog)
            return false;

        const control = support.messageActionsControl();
        if (control && control.pinned)
            return true;

        if (rootItem.walkModeActive
                || rootItem.selectedEventIds.length > 0
                || rootItem.hasFocusedEvent
                || rootItem.hasPendingAttachments
                || TimelineManager.matrixTimelineReplyEventId.length > 0
                || rootItem.editing) {
            return true;
        }

        if (rootItem.headerSearchHasFocus || rootItem.perfDisableComposer)
            return false;

        return !!(composerPane && composerPane.composerInput);
    }

    function handleEscape() {
        if (!support.canHandleEscape())
            return false;

        if (support.dismissPinnedMessageActions())
            return true;

        if (rootItem.selectedEventIds.length > 0) {
            rootItem.clearSelectedEvents();
            rootItem.focusTimelineSelection();
            return true;
        }

        if (rootItem.walkModeActive || rootItem.hasFocusedEvent) {
            rootItem.exitWalkMode({
                "focusComposer": true
            });
            return true;
        }

        if (rootItem.hasPendingAttachments) {
            TimelineManager.clearActiveMatrixAttachments();
            return true;
        }

        if (TimelineManager.matrixTimelineReplyEventId.length > 0) {
            TimelineManager.clearActiveMatrixReply();
            return true;
        }

        if (TimelineManager.matrixTimelineThreadEventId.length > 0) {
            TimelineManager.clearActiveMatrixThread();
            return true;
        }

        if (rootItem.editing) {
            TimelineManager.clearActiveMatrixEdit();
            return true;
        }

        return support.focusTextInput();
    }

    function shouldRouteTextKeyToComposer(event) {
        if (!event
                || rootItem.walkModeActive
                || rootItem.headerSearchHasFocus
                || rootItem.hasOpenOverlayDialog
                || support.composerHasTextInputFocus()) {
            return false;
        }

        const text = String(event.text || "");
        if (!support.hasInsertableComposerText(text))
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
        if (VoiceRecorder.recording || VoiceRecorder.paused) {
            VoiceRecorder.stopRecording();
        }

        if (rootItem.hasPendingAttachments) {
            // Set audio duration and voice metadata on the attachment before sending
            if (VoiceRecorder.durationMs > 0) {
                TimelineManager.setActiveAttachmentDurationMs(VoiceRecorder.durationMs);
                TimelineManager.setActiveAttachmentVoiceWaveform(
                    VoiceRecorder.normalizedWaveform(256));
            }
            // Release recorder state without deleting the temp file (upload queue owns it now)
            VoiceRecorder.releaseRecording();
            return TimelineManager.sendActiveMatrixAttachments();
        }

        const body = composerPane.composerInput.text;
        const mentions = composerInputController && composerInputController.mentions
            ? composerInputController.mentions
            : [];
        if (!rootItem.editing) {
            const inspection = TimelineManager.inspectActiveMatrixSlashCommand(body);
            const submitAction = String((inspection && inspection.submitAction) || "none");

            if (submitAction === "preserveComposer")
                return false;

            if (submitAction === "executeCommand") {
                const commandOk = TimelineManager.executeActiveMatrixSlashCommand(body, mentions);
                if (!commandOk)
                    return false;

                support.setComposerText("");
                support.focusTextInput();
                return true;
            }
        }

        const ok = rootItem.editing
            ? TimelineManager.sendActiveMatrixEditMessage(body, mentions)
            : TimelineManager.sendActiveMatrixTextMessage(body, mentions);
        if (!ok)
            return false;

        if (Settings.resolvedComposerTypingSendEnabled(rootItem.activeRoomId))
            TimelineManager.sendActiveMatrixTypingNotice(false);

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

    function openRemoveMessageDialog(eventId, transactionId) {
        return support.dialogSupport.openRemoveMessageDialog(eventId, transactionId);
    }

    function openRawMessageDialog(eventId) {
        return support.dialogSupport.openRawMessageDialog(eventId);
    }

    function openReadReceiptsDialog(eventId) {
        return support.dialogSupport.openReadReceiptsDialog(eventId);
    }

    function openReactionDetailsDialog(eventId, reactions) {
        return support.dialogSupport.openReactionDetailsDialog(eventId, reactions);
    }

    function openMatrixForwardDialog(eventId) {
        return support.dialogSupport.openMatrixForwardDialog(eventId);
    }

    function openForwardDialog(eventId) {
        return support.dialogSupport.openMatrixForwardDialog(eventId);
    }

    function openForwardDialogForEvents(eventIds, selectionCount) {
        return support.dialogSupport.openForwardDialogForEvents(eventIds, selectionCount);
    }

    function openRemoveMessagesDialog(eventIds, selectionCount) {
        return support.dialogSupport.openRemoveMessagesDialog(eventIds, selectionCount);
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
                                      roomModelOverride,
                                      transactionId) {
        return support.dialogSupport.openMessageActionsDialog(eventId,
                                                              threadId,
                                                              eventType,
                                                              isSender,
                                                              isEncrypted,
                                                              isEditable,
                                                              link,
                                                              text,
                                                              messageModelOverride,
                                                              roomModelOverride,
                                                              transactionId);
    }

    // The pending jump we last opened a thread view for. Opening is a
    // one-shot per jump: retrying on every state change would fight the
    // user if they close the thread while the jump is still pending.
    property string pendingJumpThreadOpenedFor: ""

    function resolvePendingMatrixEventJump() {
        const pendingEventId = String(TimelineManager.matrixTimelinePendingJumpEventId || "").trim();
        if (pendingEventId.length === 0)
            return false;

        if (!TimelineManager.resolveActiveMatrixPendingJump())
            return false;

        if (support.jumpToLoadedMatrixEvent(pendingEventId)) {
            TimelineManager.clearActiveMatrixPendingJump(pendingEventId);
            support.pendingJumpThreadOpenedFor = "";
            return true;
        }

        // The event is loaded in the room timeline but the bound view can't
        // show it: a thread reply while replies are collapsed (or while a
        // different thread view is open). Open its thread and finish the
        // jump when the thread timeline snapshot includes the event.
        const roomModel = rootItem.perRoomModel;
        const row = roomModel ? roomModel.rowForEventId(pendingEventId) : -1;
        const item = row >= 0 ? roomModel.itemAt(row) : null;
        const threadId = item ? String(item.threadId || "") : "";
        if (threadId.length === 0)
            return false;

        if (support.pendingJumpThreadOpenedFor === pendingEventId)
            return false;

        support.pendingJumpThreadOpenedFor = pendingEventId;
        TimelineManager.queueActiveMatrixThread(threadId);
        return false;
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

        function onMatrixThreadTimelineChanged() {
            // A pending jump into a thread reply waits for the thread
            // timeline to load the event; thread snapshots don't fire
            // matrixTimelineStateChanged, so retry on this signal too.
            support.resolvePendingMatrixEventJump();
        }

        function onFocusInput() {
            support.focusTextInput();
        }

        function onEscapeRequested() {
            // Only the active pool entry should react: walk-mode exit and
            // composer focus would otherwise act on whichever inactive
            // pool slot also has this Connections instance attached.
            // `rootItem.roomPreview` is set only for the active pool slot.
            if (!rootItem.roomPreview)
                return;

            // Sidebar Escape goes straight to the composer rather than
            // running the full handleEscape() cascade. The cascade clears
            // a pending reply/edit/thread/attachments before focusing,
            // which is the right semantic when Escape is pressed from
            // inside the room itself but feels destructive from the
            // sidebar (the user wasn't touching the draft state).
            //
            // Walk mode is the one case where focusTextInput() no-ops:
            // the composer is hidden, so we explicitly exit walk mode
            // (which itself focuses the composer afterwards).
            if (rootItem.walkModeActive) {
                rootItem.exitWalkMode({ "focusComposer": true });
                return;
            }
            support.focusTextInput();
        }
    }
}
