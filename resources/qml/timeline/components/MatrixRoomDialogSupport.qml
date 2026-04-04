// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../dialogs/moderation" as ModerationDialogs
import "../../dialogs/navigation" as NavigationDialogs
import "../../dialogs/timeline" as TimelineDialogs
import QtQuick
import cc.etke.komai

Item {
    id: support

    required property var rootItem
    required property var roomPreview
    required property var chatRoot
    required property var timelineRoot
    required property var emojiPopup
    required property var filteredTimeline
    required property var timelineList
    required property var messageActionsDefaultRoomModel
    required property var dialogRoomModel
    required property var forwardRoomModel

    width: 0
    height: 0
    property string pendingRawMessageEventId: ""
    property string pendingReadReceiptsEventId: ""

    readonly property var messageContextMenu: matrixMessageContextMenu
    readonly property var replyContextMenu: matrixReplyContextMenu
    readonly property var messageActionsHost: matrixMessageActionsHost

    MessageContextMenu {
        id: matrixMessageContextMenu

        chatRoot: support.rootItem
        emojiPopup: support.emojiPopup
        filteredTimelineModel: support.filteredTimeline
    }

    ReplyContextMenu {
        id: matrixReplyContextMenu

        roomModel: support.messageActionsDefaultRoomModel
    }

    MessageActionsHost {
        id: matrixMessageActionsHost

        chatList: support.timelineList
        chatRoot: support.rootItem
        emojiPopup: support.emojiPopup
        filteredTimeline: support.filteredTimeline
        roomModel: support.messageActionsDefaultRoomModel
    }

    Component {
        id: removeReasonDialogComponent

        InputDialog {
            required property string eventId

            placeholderText: qsTr("Optional reason")
            title: qsTr("Delete this message?")
            titleIcon: ":/icons/icons/ui/delete.svg"
            acceptText: qsTr("Delete")

            onInputAccepted: function (text) {
                TimelineManager.redactActiveMatrixTimelineEvent(eventId, text);
                support.rootItem.exitWalkMode({
                        "focusComposer": true
                    });
            }
        }
    }

    Component {
        id: removeMultipleMessagesDialogComponent

        InputDialog {
            required property var eventIds
            required property int selectionCount

            placeholderText: qsTr("Optional reason")
            title: selectionCount > eventIds.length
                ? qsTr("Delete %1 of %2 selected messages?").arg(eventIds.length).arg(selectionCount)
                : qsTr("Delete %n selected messages?", "", eventIds.length)
            titleIcon: ":/icons/icons/ui/delete.svg"
            acceptText: qsTr("Delete")

            onInputAccepted: function (text) {
                TimelineManager.redactActiveMatrixTimelineEvents(eventIds, text);
                support.rootItem.exitWalkMode({
                        "focusComposer": true
                    });
            }
        }
    }

    Component {
        id: rawMessageDialogComponent

        TimelineDialogs.RawMessageDialog {
        }
    }

    Component {
        id: readReceiptsDialogComponent

        TimelineDialogs.ReadReceipts {
        }
    }

    Component {
        id: reportMessageDialogComponent

        ModerationDialogs.ReportMessage {
        }
    }

    Component {
        id: forwardDialogComponent

        NavigationDialogs.ForwardCompleter {
        }
    }

    Connections {
        target: TimelineManager

        function onActiveMatrixTimelineRawMessageDialogReady(eventId, payload) {
            const trimmedEventId = String(eventId || "").trim();
            if (trimmedEventId !== support.pendingRawMessageEventId)
                return;

            support.pendingRawMessageEventId = "";
            if (!payload || !payload.rawMessageJson) {
                support.showDialogFromComponent(rawMessageDialogComponent, {
                    "renderedRawMessage": qsTr("Raw JSON is not available for this event. It may have been redacted."),
                    "rawMessageJson": "",
                    "rawMessageBody": "",
                    "rawMessageFormattedBody": ""
                });
                return;
            }

            support.showDialogFromComponent(rawMessageDialogComponent, payload);
        }

        function onActiveMatrixTimelineReadReceiptsReady(eventId, readReceipts) {
            const trimmedEventId = String(eventId || "").trim();
            if (trimmedEventId !== support.pendingReadReceiptsEventId) {
                if (readReceipts && readReceipts.destroy)
                    readReceipts.destroy();
                return;
            }

            support.pendingReadReceiptsEventId = "";
            if (!readReceipts)
                return;

            const dialog = support.showDialogFromComponent(readReceiptsDialogComponent, {
                    "readReceipts": readReceipts,
                    "room": support.dialogRoomModel
                });
            if (!dialog && readReceipts.destroy)
                readReceipts.destroy();
        }
    }

    function destroyOnClose(dialog) {
        if (!dialog)
            return;

        if (support.chatRoot && support.chatRoot.dialogHost && support.chatRoot.dialogHost.destroyOnClose != undefined) {
            support.chatRoot.dialogHost.destroyOnClose(dialog);
            return;
        }

        if (dialog.closing != undefined)
            dialog.closing.connect(() => dialog.destroy(1000));
        else if (dialog.aboutToHide != undefined)
            dialog.aboutToHide.connect(() => dialog.destroy(1000));
    }

    function showDialogFromComponent(componentRef, properties) {
        const dialogParent = support.chatRoot && support.chatRoot.dialogHost
            ? support.chatRoot.dialogHost
            : (support.chatRoot ? support.chatRoot : support.rootItem);
        const dialog = componentRef.createObject(dialogParent, properties || {});
        if (!dialog)
            return null;

        dialog.open();
        support.rootItem.openOverlayDialogCount += 1;
        const decrementOnce = (function() {
            let done = false;
            return function() {
                if (!done) {
                    done = true;
                    support.rootItem.openOverlayDialogCount = Math.max(
                        0,
                        support.rootItem.openOverlayDialogCount - 1);
                }
            };
        })();
        if (dialog.closing !== undefined)
            dialog.closing.connect(decrementOnce);
        else if (dialog.aboutToHide !== undefined)
            dialog.aboutToHide.connect(decrementOnce);
        support.destroyOnClose(dialog);
        return dialog;
    }

    function openRemoveMessageDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        return showDialogFromComponent(removeReasonDialogComponent, {
                "eventId": trimmedEventId
            });
    }

    function openRawMessageDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        support.pendingRawMessageEventId = "";
        if (!TimelineManager.requestRawMessageDialogForActiveMatrixTimelineEvent(trimmedEventId))
            return null;

        support.pendingRawMessageEventId = trimmedEventId;
        return null;
    }

    function openReadReceiptsDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        support.pendingReadReceiptsEventId = "";
        if (!TimelineManager.requestReadReceiptsModelForActiveMatrixTimelineEvent(trimmedEventId))
            return null;

        support.pendingReadReceiptsEventId = trimmedEventId;
        return null;
    }

    function openMatrixForwardDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        if (support.chatRoot && support.chatRoot.dialogHost
                && typeof support.chatRoot.dialogHost.showForwardMessageDialog === "function") {
            return support.chatRoot.dialogHost.showForwardMessageDialog(
                support.forwardRoomModel,
                [trimmedEventId],
                null,
                null,
                1);
        }

        const dialogParent = support.chatRoot && support.chatRoot.dialogHost
            ? support.chatRoot.dialogHost
            : (support.chatRoot ? support.chatRoot : support.rootItem);
        const dialog = forwardDialogComponent.createObject(dialogParent, {
                "roomSource": support.forwardRoomModel,
                "dialogViewportWidth": dialogParent && dialogParent.width !== undefined
                    ? Number(dialogParent.width)
                    : support.rootItem.width,
                "modalOverlayColor": support.timelineRoot
                    && support.timelineRoot.overlayBackdropColor !== undefined
                    ? support.timelineRoot.overlayBackdropColor
                    : Qt.rgba(0, 0, 0,
                              support.rootItem.palette.window.hslLightness < 0.5 ? 0.76 : 0.68),
                "timelineSource": null,
                "timelineViewSource": null,
                "showReplyPreview": true
            });
        if (!dialog)
            return null;

        dialog.setMessageEventIds([trimmedEventId], 1);
        dialog.open();
        support.destroyOnClose(dialog);
        return dialog;
    }

    function openForwardDialogForEvents(eventIds, selectionCount) {
        if (!eventIds || eventIds.length === 0)
            return null;

        if (support.chatRoot && support.chatRoot.dialogHost
                && typeof support.chatRoot.dialogHost.showForwardMessageDialog === "function") {
            return support.chatRoot.dialogHost.showForwardMessageDialog(
                support.forwardRoomModel,
                eventIds,
                null,
                null,
                selectionCount);
        }

        const dialogParent = support.chatRoot && support.chatRoot.dialogHost
            ? support.chatRoot.dialogHost
            : (support.chatRoot ? support.chatRoot : support.rootItem);
        const dialog = forwardDialogComponent.createObject(dialogParent, {
                "roomSource": support.forwardRoomModel,
                "dialogViewportWidth": dialogParent && dialogParent.width !== undefined
                    ? Number(dialogParent.width)
                    : support.rootItem.width,
                "modalOverlayColor": support.timelineRoot
                    && support.timelineRoot.overlayBackdropColor !== undefined
                    ? support.timelineRoot.overlayBackdropColor
                    : Qt.rgba(0, 0, 0,
                              support.rootItem.palette.window.hslLightness < 0.5 ? 0.76 : 0.68),
                "timelineSource": null,
                "timelineViewSource": null,
                "showReplyPreview": true
            });
        if (!dialog)
            return null;

        dialog.setMessageEventIds(eventIds, selectionCount);
        dialog.open();
        support.destroyOnClose(dialog);
        return dialog;
    }

    function openRemoveMessagesDialog(eventIds, selectionCount) {
        if (!eventIds || eventIds.length === 0)
            return null;

        return showDialogFromComponent(removeMultipleMessagesDialogComponent, {
                "eventIds": eventIds,
                "selectionCount": selectionCount
            });
    }

    function openReportMessageDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        return showDialogFromComponent(reportMessageDialogComponent, {
                "eventId": trimmedEventId,
                "room": support.messageActionsDefaultRoomModel
            });
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
        const component = Qt.createComponent("qrc:/resources/qml/dialogs/timeline/MessageActionsDialog.qml");
        if (component.status !== Component.Ready) {
            console.error("MessageActionsDialog: " + component.errorString());
            return null;
        }

        const dialogParent = support.chatRoot && support.chatRoot.dialogHost
            ? support.chatRoot.dialogHost
            : (support.chatRoot ? support.chatRoot : support.rootItem);
        const dialog = component.createObject(dialogParent, {
                "eventId": eventId,
                "eventType": eventType,
                "isSender": isSender,
                "isEncrypted": isEncrypted,
                "link": link || "",
                "roomModel": roomModelOverride || support.messageActionsDefaultRoomModel,
                "roomModelOverride": roomModelOverride || null,
                "messageModelOverride": messageModelOverride || null,
                "chatRoot": support.rootItem,
                "appRoot": dialogParent
            });
        if (!dialog)
            return null;

        dialog.open();
        support.destroyOnClose(dialog);
        return dialog;
    }
}
