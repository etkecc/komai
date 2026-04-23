// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Menu {
    id: messageContextMenuRoot

    required property var chatRoot
    required property var emojiPopup
    required property var filteredTimelineModel
    property var roomModel: null
    property var actionMessageModel: null
    property var actionRoomModel: null
    property string eventId
    property string transactionId
    property int eventType
    property bool isEditable
    property bool isEncrypted
    property bool isSender
    property bool isStateEvent
    property string link
    property string text
    property string threadId
    // Derived: true when this menu targets a local echo (no server event_id yet,
    // only a transaction id). Most actions (reply, edit, pin, ...) require a real
    // event id and must be hidden for these rows.
    readonly property bool isLocalEcho: (!eventId || eventId.length === 0)
        && transactionId.length > 0
    property Item popupAnchorItem: null
    property Item lastClosedAnchorItem: null
    property string lastClosedEventId: ""
    property double lastClosedAtMs: 0
    property int closedReuseIgnoreMs: 250
    readonly property var effectiveRoomModel: actionRoomModel
        ? actionRoomModel
        : (effectiveMessageModel && effectiveMessageModel.roomModelOverride)
            ? effectiveMessageModel.roomModelOverride
            : roomModel
    readonly property var effectiveMessageModel: actionMessageModel ? actionMessageModel : ({
            "eventId": eventId,
            // Menu callers pass the real event id in via show()'s first arg, so in
            // the non-wrapper fallback it coincides with `eventId`. Wrappers expose
            // `realEventId` distinctly from their lookup-key `eventId`.
            "realEventId": eventId,
            "transactionId": transactionId,
            "isLocalEcho": isLocalEcho,
            "threadId": threadId,
            "type": eventType,
            "isSender": isSender,
            "isEncrypted": isEncrypted,
            "isEditable": isEditable,
            "isStateEvent": isStateEvent
        })
    readonly property bool hasFormattedBody: effectiveRoomModel
        && typeof effectiveRoomModel.dataById === "function"
        && !!effectiveRoomModel.dataById(eventId, Room.HasFormattedBody, false)

    function wasJustClosedFor(eventId_, anchor_) {
        if (!eventId_ || !anchor_)
            return false;

        return lastClosedEventId === eventId_ && lastClosedAnchorItem === anchor_ && (Date.now() - lastClosedAtMs) <= closedReuseIgnoreMs;
    }

    function show(eventId_, threadId_, eventType_, isSender_, isEncrypted_, isEditable_, isStateEvent_, link_, text_, showAt_, actionMessageModel_, actionRoomModel_, transactionId_) {
        eventId = eventId_;
        transactionId = transactionId_ || "";
        threadId = threadId_;
        eventType = eventType_;
        isEncrypted = isEncrypted_;
        isEditable = isEditable_;
        isSender = isSender_;
        isStateEvent = isStateEvent_;
        popupAnchorItem = showAt_ || null;
        actionMessageModel = actionMessageModel_ || null;
        actionRoomModel = actionRoomModel_ || null;
        if (text_)
            text = text_;
        else
            text = "";
        if (link_)
            link = link_;
        else
            link = "";

        messageActionsFilter.updateTarget();

        if (showAt_) {
            popup(showAt_);
        } else {
            popupAnchorItem = null;
            popup();
        }
    }

    Component {
        id: reportDialog

        ReportMessage {
        }
    }

    Component.onCompleted: {
        if (messageContextMenuRoot.popupType != undefined)
            messageContextMenuRoot.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
    }
    onClosed: {
        lastClosedAtMs = Date.now();
        lastClosedEventId = eventId;
        lastClosedAnchorItem = popupAnchorItem;
        popupAnchorItem = null;
        actionMessageModel = null;
        actionRoomModel = null;
    }

    MessageActionSupport {
        id: messageActionSupport
    }

    KomaiMenuVisibilityFilter on contentData {
        id: messageActionsFilter

        // --- Compose section ---
        Component {
            MenuItem {
                text: qsTr("Repl&y")
                icon.source: "qrc:/icons/icons/ui/reply.svg"
                visible: messageActionSupport.canReply(messageContextMenuRoot.effectiveMessageModel,
                                                       messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.reply = (messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("Reply in &Thread")
                icon.source: "qrc:/icons/icons/ui/thread.svg"
                visible: messageActionSupport.canThread(messageContextMenuRoot.effectiveMessageModel,
                                                        messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.thread = (messageContextMenuRoot.threadId || messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Edit")
                icon.source: "qrc:/icons/icons/ui/edit.svg"
                visible: messageActionSupport.canEdit(messageContextMenuRoot.effectiveMessageModel,
                                                      messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.edit = (messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("Re&act")
                icon.source: "qrc:/icons/icons/ui/smile-add.svg"
                visible: messageActionSupport.canReact(messageContextMenuRoot.effectiveMessageModel,
                                                       messageContextMenuRoot.effectiveRoomModel)

                // Capture targets into locals: triggering the MenuItem closes
                // the Menu and `onClosed` nulls out actionRoomModel, so the
                // async emoji-pick callback must not read effectiveRoomModel.
                onTriggered: {
                    if (emojiPopup.visible) {
                        emojiPopup.close();
                        return;
                    }
                    const targetRoom = messageContextMenuRoot.effectiveRoomModel;
                    const targetEventId = messageContextMenuRoot.eventId;
                    if (!targetRoom)
                        return;
                    emojiPopup.show(null, targetRoom.roomId, function (plaintext, markdown) {
                        if (targetRoom && targetRoom.input)
                            targetRoom.input.reaction(targetEventId, plaintext);
                        TimelineManager.focusMessageInput();
                    });
                }
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Forward")
                icon.source: "qrc:/icons/icons/ui/forward.svg"
                visible: messageActionSupport.canForward(messageContextMenuRoot.effectiveMessageModel)

                onTriggered: messageActionSupport.applyForward(messageContextMenuRoot.chatRoot,
                                                               messageContextMenuRoot.effectiveRoomModel,
                                                               messageContextMenuRoot.effectiveMessageModel)
            }
        }

        // --- Separator: Compose / Clipboard ---
        Component {
            MenuSeparator {
                visible: messageActionSupport.canReply(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
                    || messageActionSupport.canEdit(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
                    || messageActionSupport.canThread(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
                    || messageActionSupport.canReact(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
                    || messageActionSupport.canForward(messageContextMenuRoot.effectiveMessageModel)
            }
        }

        // --- Clipboard section ---
        Component {
            MenuItem {
                text: qsTr("&Copy")
                icon.source: "qrc:/icons/icons/ui/copy.svg"
                visible: messageContextMenuRoot.text

                onTriggered: Clipboard.text = messageContextMenuRoot.text
            }
        }
        Component {
            MenuItem {
                text: qsTr("Copy formatted text")
                icon.source: "qrc:/icons/icons/ui/copy.svg"
                visible: messageContextMenuRoot.hasFormattedBody

                onTriggered: Clipboard.text = messageContextMenuRoot.effectiveRoomModel.dataById(messageContextMenuRoot.eventId, Room.FormattedBody, "")
            }
        }
        Component {
            MenuItem {
                text: qsTr("Copy &link location")
                icon.source: "qrc:/icons/icons/ui/link.svg"
                visible: messageContextMenuRoot.link

                onTriggered: Clipboard.text = messageContextMenuRoot.link
            }
        }
        Component {
            MenuItem {
                text: qsTr("Copy link to eve&nt")
                icon.source: "qrc:/icons/icons/ui/link.svg"
                visible: messageActionSupport.canCopyEventLink(messageContextMenuRoot.effectiveMessageModel,
                                                               messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.copyLinkToEvent(messageContextMenuRoot.eventId)
            }
        }

        // --- Separator: Clipboard / Manage ---
        Component {
            MenuSeparator {
                visible: messageContextMenuRoot.text
                    || messageContextMenuRoot.hasFormattedBody
                    || messageContextMenuRoot.link
                    || messageActionSupport.canCopyEventLink(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
            }
        }

        // --- Manage section ---
        Component {
            MenuItem {
                readonly property bool isPinned: messageContextMenuRoot.effectiveRoomModel
                    && messageContextMenuRoot.effectiveRoomModel.pinnedMessages
                    && messageContextMenuRoot.effectiveRoomModel.pinnedMessages.includes(messageContextMenuRoot.eventId)

                text: isPinned ? qsTr("Un&pin") : qsTr("&Pin")
                icon.source: isPinned ? "qrc:/icons/icons/ui/pin-off.svg" : "qrc:/icons/icons/ui/pin.svg"
                visible: messageActionSupport.canPin(messageContextMenuRoot.effectiveMessageModel,
                                                     messageContextMenuRoot.effectiveRoomModel)

                onTriggered: {
                    if (isPinned)
                        messageContextMenuRoot.effectiveRoomModel.unpin(messageContextMenuRoot.eventId);
                    else
                        messageContextMenuRoot.effectiveRoomModel.pin(messageContextMenuRoot.eventId);
                }
            }
        }
        Component {
            MenuItem {
                text: qsTr("Go to &message")
                icon.source: "qrc:/icons/icons/ui/go-to.svg"
                visible: messageActionSupport.canGoToMessage(messageContextMenuRoot.effectiveMessageModel,
                                                             filteredTimelineModel)

                onTriggered: function () {
                    if (!messageContextMenuRoot.eventId)
                        return;

                    chatRoot.clearSearch();
                    messageContextMenuRoot.effectiveRoomModel.showEvent(messageContextMenuRoot.eventId);
                }
            }
        }

        // --- Separator: Manage / Media ---
        Component {
            MenuSeparator {
                visible: messageActionSupport.canPin(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
                    || messageActionSupport.canGoToMessage(messageContextMenuRoot.effectiveMessageModel, filteredTimelineModel)
            }
        }

        // --- Media section ---
        Component {
            MenuItem {
                text: qsTr("&Save as")
                icon.source: "qrc:/icons/icons/ui/download.svg"
                visible: messageActionSupport.canSaveMedia(messageContextMenuRoot.effectiveMessageModel,
                                                           messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.saveMedia(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Open in external program")
                icon.source: "qrc:/icons/icons/ui/open-externally.svg"
                visible: messageActionSupport.canOpenMedia(messageContextMenuRoot.effectiveMessageModel,
                                                           messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.openMedia(messageContextMenuRoot.eventId)
            }
        }

        // --- Separator: Media / Inspect ---
        Component {
            MenuSeparator {
                visible: messageActionSupport.canSaveMedia(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
                    || messageActionSupport.canOpenMedia(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
            }
        }

        // --- Inspect section ---
        Component {
            MenuItem {
                text: qsTr("&Read receipts")
                icon.source: "qrc:/icons/icons/ui/eye-show.svg"
                visible: messageActionSupport.canReadReceipts(messageContextMenuRoot.effectiveMessageModel,
                                                              messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.showReadReceipts(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("View raw message")
                icon.source: "qrc:/icons/icons/ui/raw-message.svg"
                visible: messageActionSupport.canViewRaw(messageContextMenuRoot.effectiveMessageModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.viewRawMessage(messageContextMenuRoot.eventId)
            }
        }

        // --- Separator: Inspect / Moderate ---
        Component {
            MenuSeparator {
                visible: messageActionSupport.canReadReceipts(messageContextMenuRoot.effectiveMessageModel, messageContextMenuRoot.effectiveRoomModel)
                    || messageActionSupport.canViewRaw(messageContextMenuRoot.effectiveMessageModel)
            }
        }

        // --- Moderate section ---
        Component {
            MenuItem {
                text: qsTr("Report message")
                icon.source: "qrc:/icons/icons/ui/alert.svg"
                visible: messageActionSupport.canReport(messageContextMenuRoot.effectiveMessageModel, chatRoot)

                onTriggered: function () {
                    chatRoot.showDialogFromComponent(reportDialog, {
                            "eventId": messageContextMenuRoot.eventId,
                            "room": messageContextMenuRoot.effectiveRoomModel
                        });
                }
            }
        }
        Component {
            MenuItem {
                text: messageContextMenuRoot.isLocalEcho
                    ? qsTr("&Cancel send")
                    : qsTr("&Delete message")
                icon.source: "qrc:/icons/icons/ui/delete.svg"
                visible: messageActionSupport.canRemove(messageContextMenuRoot.effectiveMessageModel,
                                                        messageContextMenuRoot.effectiveRoomModel)

                onTriggered: chatRoot.openRemoveMessageDialog(messageContextMenuRoot.eventId,
                                                              messageContextMenuRoot.transactionId)
            }
        }
    }
}
