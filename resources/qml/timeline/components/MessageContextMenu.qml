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
    property int eventType
    property bool isEditable
    property bool isEncrypted
    property bool isSender
    property bool isStateEvent
    property string link
    property string text
    property string threadId
    property Item popupAnchorItem: null
    property Item lastClosedAnchorItem: null
    property string lastClosedEventId: ""
    property double lastClosedAtMs: 0
    property int closedReuseIgnoreMs: 250
    readonly property var effectiveRoomModel: actionRoomModel ? actionRoomModel : roomModel
    readonly property var effectiveMessageModel: actionMessageModel ? actionMessageModel : ({
            "eventId": eventId,
            "threadId": threadId,
            "type": eventType,
            "isSender": isSender,
            "isEncrypted": isEncrypted,
            "isEditable": isEditable,
            "isStateEvent": isStateEvent
        })

    function wasJustClosedFor(eventId_, anchor_) {
        if (!eventId_ || !anchor_)
            return false;

        return lastClosedEventId === eventId_ && lastClosedAnchorItem === anchor_ && (Date.now() - lastClosedAtMs) <= closedReuseIgnoreMs;
    }

    function show(eventId_, threadId_, eventType_, isSender_, isEncrypted_, isEditable_, isStateEvent_, link_, text_, showAt_, actionMessageModel_, actionRoomModel_) {
        eventId = eventId_;
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

        Component {
            MenuItem {
                text: qsTr("Go to &message")
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
        Component {
            MenuItem {
                text: qsTr("&Copy")
                visible: messageContextMenuRoot.text

                onTriggered: Clipboard.text = messageContextMenuRoot.text
            }
        }
        Component {
            MenuItem {
                text: qsTr("Copy &link location")
                visible: messageContextMenuRoot.link

                onTriggered: Clipboard.text = messageContextMenuRoot.link
            }
        }
        Component {
            MenuItem {
                id: reactionOption

                text: qsTr("Re&act")
                visible: messageActionSupport.canReact(messageContextMenuRoot.effectiveMessageModel,
                                                       messageContextMenuRoot.effectiveRoomModel)

                onTriggered: emojiPopup.visible ? emojiPopup.close() : emojiPopup.show(null, messageContextMenuRoot.effectiveRoomModel.roomId, function (plaintext, markdown) {
                    messageContextMenuRoot.effectiveRoomModel.input.reaction(messageContextMenuRoot.eventId, plaintext);
                    TimelineManager.focusMessageInput();
                })
            }
        }
        Component {
            MenuItem {
                text: qsTr("Repl&y")
                visible: messageActionSupport.canReply(messageContextMenuRoot.effectiveMessageModel,
                                                       messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.reply = (messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Edit")
                visible: messageActionSupport.canEdit(messageContextMenuRoot.effectiveMessageModel,
                                                      messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.edit = (messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Thread")
                visible: messageActionSupport.canThread(messageContextMenuRoot.effectiveMessageModel,
                                                        messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.thread = (messageContextMenuRoot.threadId || messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: visible && messageContextMenuRoot.effectiveRoomModel.pinnedMessages.includes(messageContextMenuRoot.eventId) ? qsTr("Un&pin") : qsTr("&Pin")
                visible: messageActionSupport.canPin(messageContextMenuRoot.effectiveMessageModel,
                                                     messageContextMenuRoot.effectiveRoomModel)

                onTriggered: visible && messageContextMenuRoot.effectiveRoomModel.pinnedMessages.includes(messageContextMenuRoot.eventId)
                    ? messageContextMenuRoot.effectiveRoomModel.unpin(messageContextMenuRoot.eventId)
                    : messageContextMenuRoot.effectiveRoomModel.pin(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Read receipts")
                visible: messageActionSupport.canReadReceipts(messageContextMenuRoot.effectiveMessageModel,
                                                              messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.showReadReceipts(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Forward")
                visible: messageActionSupport.canForward(messageContextMenuRoot.effectiveMessageModel)

                onTriggered: chatRoot.openForwardDialog(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Mark as read")
                visible: messageActionSupport.canMarkAsRead(messageContextMenuRoot.effectiveMessageModel,
                                                            messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.markEventAsRead(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("View raw message")
                visible: messageActionSupport.canViewRaw(messageContextMenuRoot.effectiveMessageModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.viewRawMessage(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("View decrypted raw message")
                // TODO(Nico): Fix this still being iterated over, when using keyboard to select options
                visible: messageContextMenuRoot.isEncrypted
                    && messageActionSupport.canViewRaw(messageContextMenuRoot.effectiveMessageModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.viewDecryptedRawMessage(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Delete message")
                visible: messageActionSupport.canRemove(messageContextMenuRoot.effectiveMessageModel,
                                                        messageContextMenuRoot.effectiveRoomModel)

                onTriggered: chatRoot.openRemoveMessageDialog(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("Report message")
                visible: messageActionSupport.canReport(messageContextMenuRoot.effectiveMessageModel, chatRoot)

                onTriggered: function () {
                    chatRoot.showDialogFromComponent(reportDialog, {
                            "eventId": messageContextMenuRoot.eventId
                        });
                }
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Save as")
                visible: messageActionSupport.canSaveMedia(messageContextMenuRoot.effectiveMessageModel,
                                                           messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.saveMedia(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Open in external program")
                visible: messageActionSupport.canOpenMedia(messageContextMenuRoot.effectiveMessageModel,
                                                           messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.openMedia(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("Copy link to eve&nt")
                visible: messageActionSupport.canCopyEventLink(messageContextMenuRoot.effectiveMessageModel,
                                                               messageContextMenuRoot.effectiveRoomModel)

                onTriggered: messageContextMenuRoot.effectiveRoomModel.copyLinkToEvent(messageContextMenuRoot.eventId)
            }
        }
    }
}
