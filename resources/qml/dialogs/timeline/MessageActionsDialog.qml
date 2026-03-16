// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../delegates/"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property string eventId
    property int eventType
    property bool isSender
    property bool isEncrypted
    property string link
    property var appRoot

    required property var roomModel
    required property var chatRoot

    readonly property string messageText: (eventId && roomModel) ? String(roomModel.dataById(eventId, Room.Body, "") || "") : ""
    readonly property string formattedBodyText: (eventId && roomModel) ? String(roomModel.dataById(eventId, Room.FormattedBody, "") || "") : ""
    readonly property bool hasFormattedBody: (eventId && roomModel) ? !!roomModel.dataById(eventId, Room.HasFormattedBody, false) : false
    readonly property bool isStateEvent: (eventId && roomModel) ? !!roomModel.dataById(eventId, Room.IsStateEvent, false) : false

    readonly property bool canRedact: roomModel ? roomModel.permissions.canRedact() : false
    readonly property bool canChangePinned: roomModel ? roomModel.permissions.canChange(MtxEvent.PinnedEvents) : false
    readonly property bool isMediaType: eventType == MtxEvent.ImageMessage
        || eventType == MtxEvent.VideoMessage
        || eventType == MtxEvent.AudioMessage
        || eventType == MtxEvent.FileMessage
        || eventType == MtxEvent.Sticker
    readonly property bool isTextType: eventType == MtxEvent.TextMessage
        || eventType == MtxEvent.EmoteMessage
        || eventType == MtxEvent.NoticeMessage
    readonly property bool isPinned: roomModel && roomModel.pinnedMessages.includes(eventId)

    overlayViewport: appRoot
    readonly property int dialogViewportWidth: overlayDialogViewport ? overlayDialogViewport.width : 760
    readonly property int dialogViewportHeight: overlayDialogViewport ? overlayDialogViewport.height : 600

    width: Math.min(
        Math.max(240, dialogViewportWidth - Komai.paddingLarge * 2),
        Math.max(240, Math.floor(dialogViewportWidth * overlayDialogMaxWidthRatio))
    )
    x: Math.round((dialogViewportWidth - width) / 2)
    y: Math.max(Komai.paddingLarge, Math.round((dialogViewportHeight - height) / 2))
    title: qsTr("Message actions")
    titleIcon: ":/icons/icons/ui/options-circle.svg"

    component ActionButton: Components.KomaiActionRowButton {
        id: actionBtn

        property string shortcutSequence: ""
        property string shortcutDisplayText: ""

        property bool _showingFeedback: false
        property string _feedbackText: ""

        function showFeedback(text) {
            _feedbackText = text;
            _showingFeedback = true;
            _feedbackTimer.restart();
        }

        Timer {
            id: _feedbackTimer
            interval: 1500
            onTriggered: actionBtn._showingFeedback = false
        }

        displayLabelText: actionBtn._showingFeedback ? actionBtn._feedbackText : actionBtn.labelText
        displayIconSource: actionBtn._showingFeedback ? ":/icons/icons/ui/checkmark.svg" : actionBtn.iconSource
        displayMirrorIcon: actionBtn._showingFeedback ? false : actionBtn.mirrorIcon

        Shortcut {
            enabled: root.visible && actionBtn.visible && actionBtn.shortcutSequence !== ""
            sequence: actionBtn.shortcutSequence
            context: Qt.ApplicationShortcut

            onActivated: actionBtn.clicked()
        }

        trailingContent: Components.ShortcutKeyBadge {
            text: actionBtn.shortcutDisplayText
            highlighted: actionBtn.activeState
            showKeyboardIcon: true
            liveModifierHighlight: true
            keyTextColor: actionBtn.foregroundColor
            visible: !actionBtn._showingFeedback
        }
    }

    ScrollView {
        id: actionsScrollView

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: Math.min(scrollContent.implicitHeight, root.parent ? root.parent.height * 0.85 : 600)
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: scrollContent
            width: actionsScrollView.availableWidth
            spacing: Komai.paddingSmall

            // Message preview
            Reply {
                id: replyPreview

                Layout.fillWidth: true
                Layout.maximumHeight: Math.min(root.parent ? root.parent.height * 0.4 : 300, 300)
                clip: true
                enabled: false
                eventId: root.eventId
                room_: root.roomModel
                maxWidth: actionsScrollView.availableWidth

                property bool isReplyFromCurrentUser: {
                    const currentUser = Komai.currentUser;
                    const currentUserId = (currentUser && currentUser.userid)
                            ? String(currentUser.userid)
                            : "";
                    return currentUserId.length > 0 && replyPreview.userId === currentUserId;
                }
                userColor: isReplyFromCurrentUser
                    ? Komai.theme.userColorSelf
                    : root.roomModel ? TimelineManager.roomUserColor(root.roomModel.roomId, replyPreview.userId, palette.window, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyPreview.userId, palette.window)
                roomColor: isReplyFromCurrentUser
                    ? Komai.theme.userColorSelf
                    : root.roomModel ? TimelineManager.roomUserColor(root.roomModel.roomId, replyPreview.userId, palette.base, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyPreview.userId, palette.base)

                // Gradient fade when preview is clipped by maximumHeight
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 60
                    visible: replyPreview.implicitHeight > replyPreview.height
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: palette.base }
                    }
                }
            }

            // --- Clipboard section ---
            Components.SettingsSection {
                label: qsTr("Clipboard")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                visible: copyTextBtn.visible || copyFormattedTextBtn.visible || copyMediaBtn.visible || copyLinkLocationBtn.visible || copyPermalinkBtn.visible
            }

            ActionButton {
                id: copyTextBtn
                labelText: qsTr("Copy text")
                iconSource: ":/icons/icons/ui/copy.svg"
                shortcutSequence: "Alt+C"
                shortcutDisplayText: qsTr("Alt+C")
                visible: root.isTextType && root.messageText !== ""
                onClicked: {
                    Clipboard.text = root.messageText;
                    showFeedback(qsTr("Copied!"));
                }
            }

            ActionButton {
                id: copyFormattedTextBtn
                labelText: qsTr("Copy formatted text")
                iconSource: ":/icons/icons/ui/copy.svg"
                shortcutSequence: "Alt+H"
                shortcutDisplayText: qsTr("Alt+H")
                visible: root.isTextType && root.hasFormattedBody
                onClicked: {
                    Clipboard.text = root.formattedBodyText;
                    showFeedback(qsTr("Copied!"));
                }
            }

            ActionButton {
                id: copyMediaBtn
                labelText: qsTr("Copy")
                iconSource: ":/icons/icons/ui/copy.svg"
                shortcutSequence: "Alt+C"
                shortcutDisplayText: qsTr("Alt+C")
                visible: root.isMediaType
                onClicked: {
                    root.roomModel.copyMedia(root.eventId);
                    showFeedback(qsTr("Copied!"));
                }
            }

            ActionButton {
                id: copyLinkLocationBtn
                labelText: qsTr("Copy link location")
                iconSource: ":/icons/icons/ui/copy.svg"
                shortcutSequence: "Alt+L"
                shortcutDisplayText: qsTr("Alt+L")
                visible: root.link !== ""
                onClicked: {
                    Clipboard.text = root.link;
                    showFeedback(qsTr("Copied!"));
                }
            }

            ActionButton {
                id: copyPermalinkBtn
                labelText: qsTr("Copy permalink")
                iconSource: ":/icons/icons/ui/link.svg"
                shortcutSequence: "Alt+K"
                shortcutDisplayText: qsTr("Alt+K")
                visible: root.eventId !== "" && !root.isStateEvent
                onClicked: {
                    root.roomModel.copyLinkToEvent(root.eventId);
                    showFeedback(qsTr("Copied!"));
                }
            }

            // --- Manage section ---
            Components.SettingsSection {
                label: qsTr("Manage")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                visible: pinBtn.visible || markReadBtn.visible
            }

            ActionButton {
                id: pinBtn
                labelText: root.isPinned ? qsTr("Unpin") : qsTr("Pin")
                iconSource: root.isPinned ? ":/icons/icons/ui/pin-off.svg" : ":/icons/icons/ui/pin.svg"
                shortcutSequence: "Alt+P"
                shortcutDisplayText: qsTr("Alt+P")
                visible: root.canChangePinned && !root.isStateEvent
                onClicked: {
                    if (root.isPinned)
                        root.roomModel.unpin(root.eventId);
                    else
                        root.roomModel.pin(root.eventId);
                    showFeedback(root.isPinned ? qsTr("Unpinned!") : qsTr("Pinned!"));
                }
            }

            ActionButton {
                id: markReadBtn
                labelText: qsTr("Mark as read")
                iconSource: ":/icons/icons/ui/double-checkmark.svg"
                shortcutSequence: "Alt+M"
                shortcutDisplayText: qsTr("Alt+M")
                visible: !root.isStateEvent
                onClicked: {
                    root.roomModel.markEventAsRead(root.eventId);
                    showFeedback(qsTr("Done!"));
                }
            }

            // --- Media section ---
            Components.SettingsSection {
                label: qsTr("Media")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                visible: saveAsBtn.visible || openExternalBtn.visible
            }

            ActionButton {
                id: saveAsBtn
                labelText: qsTr("Save as")
                iconSource: ":/icons/icons/ui/download.svg"
                shortcutSequence: "Alt+S"
                shortcutDisplayText: qsTr("Alt+S")
                visible: root.isMediaType
                onClicked: {
                    root.close();
                    root.roomModel.saveMedia(root.eventId);
                }
            }

            ActionButton {
                id: openExternalBtn
                labelText: qsTr("Open in external program")
                iconSource: ":/icons/icons/ui/open-externally.svg"
                shortcutSequence: "Alt+O"
                shortcutDisplayText: qsTr("Alt+O")
                visible: root.isMediaType
                onClicked: {
                    root.close();
                    root.roomModel.openMedia(root.eventId);
                }
            }

            // --- Inspect section ---
            Components.SettingsSection {
                label: qsTr("Inspect")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                visible: readReceiptsBtn.visible || viewRawBtn.visible || viewDecryptedRawBtn.visible
            }

            ActionButton {
                id: readReceiptsBtn
                labelText: qsTr("Read receipts")
                iconSource: ":/icons/icons/ui/eye-show.svg"
                shortcutSequence: "Alt+I"
                shortcutDisplayText: qsTr("Alt+I")
                visible: !root.isStateEvent
                onClicked: {
                    root.close();
                    root.roomModel.showReadReceipts(root.eventId);
                }
            }

            ActionButton {
                id: viewRawBtn
                labelText: qsTr("View raw message")
                iconSource: ":/icons/icons/ui/raw-message.svg"
                shortcutSequence: "Alt+U"
                shortcutDisplayText: qsTr("Alt+U")
                onClicked: {
                    root.close();
                    root.roomModel.viewRawMessage(root.eventId);
                }
            }

            ActionButton {
                id: viewDecryptedRawBtn
                labelText: qsTr("View decrypted raw message")
                iconSource: ":/icons/icons/ui/raw-message.svg"
                shortcutSequence: "Alt+E"
                shortcutDisplayText: qsTr("Alt+E")
                visible: root.isEncrypted
                onClicked: {
                    root.close();
                    root.roomModel.viewDecryptedRawMessage(root.eventId);
                }
            }

            // --- Moderate section ---
            Components.SettingsSection {
                label: qsTr("Moderate")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                visible: removeBtn.visible || reportBtn.visible
            }

            ActionButton {
                id: removeBtn
                labelText: qsTr("Remove message")
                iconSource: ":/icons/icons/ui/delete.svg"
                shortcutSequence: "Alt+D"
                shortcutDisplayText: qsTr("Alt+D")
                visible: root.canRedact || root.isSender
                onClicked: {
                    root.close();
                    root.chatRoot.openRemoveMessageDialog(root.eventId);
                }
            }

            ActionButton {
                id: reportBtn
                labelText: qsTr("Report message")
                iconSource: ":/icons/icons/ui/alert.svg"
                shortcutSequence: "Alt+R"
                shortcutDisplayText: qsTr("Alt+R")
                visible: !root.isStateEvent
                onClicked: {
                    root.close();
                    root.chatRoot.openReportMessageDialog(root.eventId);
                }
            }
        }
    }
}
