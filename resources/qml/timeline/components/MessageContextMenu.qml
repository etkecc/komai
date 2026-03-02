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
    required property var roomModel
    required property var topBar
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

    function wasJustClosedFor(eventId_, anchor_) {
        if (!eventId_ || !anchor_)
            return false;

        return lastClosedEventId === eventId_ && lastClosedAnchorItem === anchor_ && (Date.now() - lastClosedAtMs) <= closedReuseIgnoreMs;
    }

    function show(eventId_, threadId_, eventType_, isSender_, isEncrypted_, isEditable_, isStateEvent_, link_, text_, showAt_) {
        eventId = eventId_;
        threadId = threadId_;
        eventType = eventType_;
        isEncrypted = isEncrypted_;
        isEditable = isEditable_;
        isSender = isSender_;
        isStateEvent = isStateEvent_;
        popupAnchorItem = showAt_ || null;
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
        id: removeReason

        InputDialog {
            id: removeReasonDialog

            property string eventId

            prompt: qsTr("Enter reason for removal or hit enter for no reason:")
            title: qsTr("Reason for removal")

            onAccepted: function (text) {
                roomModel.redactEvent(eventId, text);
            }
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
    }

    KomaiMenuVisibilityFilter on contentData {
        id: messageActionsFilter

        Component {
            MenuItem {
                text: qsTr("Go to &message")
                visible: filteredTimelineModel.filterByContent

                onTriggered: function () {
                    topBar.searchString = "";
                    roomModel.showEvent(messageContextMenuRoot.eventId);
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
                visible: !messageContextMenuRoot.isStateEvent && (roomModel ? roomModel.permissions.canSend(MtxEvent.Reaction) : false)

                onTriggered: emojiPopup.visible ? emojiPopup.close() : emojiPopup.show(null, roomModel.roomId, function (plaintext, markdown) {
                    roomModel.input.reaction(messageContextMenuRoot.eventId, plaintext);
                    TimelineManager.focusMessageInput();
                })
            }
        }
        Component {
            MenuItem {
                text: qsTr("Repl&y")
                visible: roomModel ? roomModel.permissions.canSend(MtxEvent.TextMessage) : false

                onTriggered: roomModel.reply = (messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Edit")
                visible: messageContextMenuRoot.isEditable && (roomModel ? roomModel.permissions.canSend(MtxEvent.TextMessage) : false)

                onTriggered: roomModel.edit = (messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Thread")
                visible: roomModel ? roomModel.permissions.canSend(MtxEvent.TextMessage) : false

                onTriggered: roomModel.thread = (messageContextMenuRoot.threadId || messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: visible && roomModel.pinnedMessages.includes(messageContextMenuRoot.eventId) ? qsTr("Un&pin") : qsTr("&Pin")
                visible: roomModel ? roomModel.permissions.canChange(MtxEvent.PinnedEvents) : false

                onTriggered: visible && roomModel.pinnedMessages.includes(messageContextMenuRoot.eventId) ? roomModel.unpin(messageContextMenuRoot.eventId) : roomModel.pin(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Read receipts")

                onTriggered: roomModel.showReadReceipts(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Forward")
                visible: messageContextMenuRoot.eventType == MtxEvent.ImageMessage || messageContextMenuRoot.eventType == MtxEvent.VideoMessage || messageContextMenuRoot.eventType == MtxEvent.AudioMessage || messageContextMenuRoot.eventType == MtxEvent.FileMessage || messageContextMenuRoot.eventType == MtxEvent.Sticker || messageContextMenuRoot.eventType == MtxEvent.TextMessage || messageContextMenuRoot.eventType == MtxEvent.LocationMessage || messageContextMenuRoot.eventType == MtxEvent.EmoteMessage || messageContextMenuRoot.eventType == MtxEvent.NoticeMessage

                onTriggered: chatRoot.openForwardDialog(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Mark as read")

                onTriggered: roomModel.markEventAsRead(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("View raw message")

                onTriggered: roomModel.viewRawMessage(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("View decrypted raw message")
                // TODO(Nico): Fix this still being iterated over, when using keyboard to select options
                visible: messageContextMenuRoot.isEncrypted

                onTriggered: roomModel.viewDecryptedRawMessage(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("Remo&ve message")
                visible: (roomModel ? roomModel.permissions.canRedact() : false) || messageContextMenuRoot.isSender

                onTriggered: function () {
                    chatRoot.showDialogFromComponent(removeReason, {
                            "eventId": messageContextMenuRoot.eventId
                        });
                }
            }
        }
        Component {
            MenuItem {
                text: qsTr("Report message")
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
                visible: messageContextMenuRoot.eventType == MtxEvent.ImageMessage || messageContextMenuRoot.eventType == MtxEvent.VideoMessage || messageContextMenuRoot.eventType == MtxEvent.AudioMessage || messageContextMenuRoot.eventType == MtxEvent.FileMessage || messageContextMenuRoot.eventType == MtxEvent.Sticker

                onTriggered: roomModel.saveMedia(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("&Open in external program")
                visible: messageContextMenuRoot.eventType == MtxEvent.ImageMessage || messageContextMenuRoot.eventType == MtxEvent.VideoMessage || messageContextMenuRoot.eventType == MtxEvent.AudioMessage || messageContextMenuRoot.eventType == MtxEvent.FileMessage || messageContextMenuRoot.eventType == MtxEvent.Sticker

                onTriggered: roomModel.openMedia(messageContextMenuRoot.eventId)
            }
        }
        Component {
            MenuItem {
                text: qsTr("Copy link to eve&nt")
                visible: messageContextMenuRoot.eventId

                onTriggered: roomModel.copyLinkToEvent(messageContextMenuRoot.eventId)
            }
        }
    }
}
