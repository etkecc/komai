// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../delegates/"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Window 2.15
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property string eventId
    property int eventType
    property bool isSender
    property bool isEncrypted
    property string link
    property var appRoot
    property var roomModelOverride: null
    property var messageModelOverride: null

    required property var roomModel
    required property var chatRoot

    readonly property var effectiveRoomModel: roomModelOverride ? roomModelOverride : roomModel
    readonly property bool hasRoleDataSource: !!effectiveRoomModel
        && typeof effectiveRoomModel.dataById === "function"
    readonly property string messageText: hasRoleDataSource
        ? ((eventId && effectiveRoomModel) ? String(effectiveRoomModel.dataById(eventId, Room.Body, "") || "") : "")
        : (messageModelOverride && messageModelOverride.body !== undefined ? String(messageModelOverride.body || "") : "")
    readonly property string formattedBodyText: hasRoleDataSource
        ? ((eventId && effectiveRoomModel) ? String(effectiveRoomModel.dataById(eventId, Room.FormattedBody, "") || "") : "")
        : (messageModelOverride && messageModelOverride.formattedBody !== undefined ? String(messageModelOverride.formattedBody || "") : "")
    readonly property bool hasFormattedBody: hasRoleDataSource
        ? ((eventId && effectiveRoomModel) ? !!effectiveRoomModel.dataById(eventId, Room.HasFormattedBody, false) : false)
        : formattedBodyText !== ""
    readonly property bool isStateEvent: hasRoleDataSource
        ? ((eventId && effectiveRoomModel) ? !!effectiveRoomModel.dataById(eventId, Room.IsStateEvent, false) : false)
        : !!(messageModelOverride && messageModelOverride.isStateEvent)
    readonly property var previewMessageData: {
        if (!hasRoleDataSource
                && effectiveRoomModel
                && typeof effectiveRoomModel.previewDataForEvent === "function") {
            const preview = effectiveRoomModel.previewDataForEvent(root.eventId);
            if (preview !== undefined && preview !== null)
                return preview;
        }

        return {
            "type": root.eventType,
            "userId": messageModelOverride && messageModelOverride.userId !== undefined
                ? String(messageModelOverride.userId || "")
                : "",
            "userName": messageModelOverride && messageModelOverride.userName !== undefined
                ? String(messageModelOverride.userName || "")
                : "",
            "body": root.messageText,
            "formattedBody": root.formattedBodyText,
            "isOnlyEmoji": 0
        };
    }

    readonly property int permissionsRevision: effectiveRoomModel && effectiveRoomModel.permissions
        ? effectiveRoomModel.permissions.revision
        : 0
    readonly property bool canRedact: {
        const _ = permissionsRevision;
        return effectiveRoomModel ? effectiveRoomModel.permissions.canRedact() : false;
    }
    readonly property bool canChangePinned: {
        const _ = permissionsRevision;
        return effectiveRoomModel ? effectiveRoomModel.permissions.canChange(MtxEvent.PinnedEvents) : false;
    }
    readonly property bool isMediaType: eventType == MtxEvent.ImageMessage
        || eventType == MtxEvent.VideoMessage
        || eventType == MtxEvent.AudioMessage
        || eventType == MtxEvent.FileMessage
        || eventType == MtxEvent.Sticker
    readonly property bool isTextType: eventType == MtxEvent.TextMessage
        || eventType == MtxEvent.EmoteMessage
        || eventType == MtxEvent.NoticeMessage
    readonly property bool isPinned: effectiveRoomModel && effectiveRoomModel.pinnedMessages && effectiveRoomModel.pinnedMessages.includes(eventId)

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
    initialFocusItem: firstVisibleActionButton()

    function addVisibleAction(buttons, button) {
        if (button && button.visible !== false && button.enabled !== false)
            buttons.push(button);
    }

    function visibleActionButtons() {
        const buttons = [];
        addVisibleAction(buttons, copyTextBtn);
        addVisibleAction(buttons, copyFormattedTextBtn);
        addVisibleAction(buttons, copyMediaBtn);
        addVisibleAction(buttons, copyLinkLocationBtn);
        addVisibleAction(buttons, copyPermalinkBtn);
        addVisibleAction(buttons, pinBtn);
        addVisibleAction(buttons, markReadBtn);
        addVisibleAction(buttons, saveAsBtn);
        addVisibleAction(buttons, openExternalBtn);
        addVisibleAction(buttons, readReceiptsBtn);
        addVisibleAction(buttons, viewRawBtn);
        addVisibleAction(buttons, viewDecryptedRawBtn);
        addVisibleAction(buttons, removeBtn);
        addVisibleAction(buttons, reportBtn);
        return buttons;
    }

    function firstVisibleActionButton() {
        const buttons = visibleActionButtons();
        return buttons.length > 0 ? buttons[0] : null;
    }

    function activeActionButtonIndex() {
        const buttons = visibleActionButtons();
        let activeItem = root.contentItem ? root.contentItem.Window.activeFocusItem : null;

        for (let index = 0; index < buttons.length; index++) {
            let current = activeItem;
            while (current) {
                if (current === buttons[index])
                    return index;
                current = current.parent;
            }
        }

        return -1;
    }

    function moveActionFocus(step) {
        const buttons = visibleActionButtons();
        if (buttons.length === 0)
            return false;

        const currentIndex = activeActionButtonIndex();
        const nextIndex = currentIndex < 0
            ? (step >= 0 ? 0 : buttons.length - 1)
            : (currentIndex + step + buttons.length) % buttons.length;

        buttons[nextIndex].forceActiveFocus();
        return true;
    }

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

    Item {
        id: actionsScrollArea

        readonly property bool showScrollbar: {
            switch (Settings.uiScrollbarPolicy) {
            case Settings.ScrollbarPolicy.Always:
                return true;
            case Settings.ScrollbarPolicy.Never:
                return false;
            case Settings.ScrollbarPolicy.WhenNeeded:
            default:
                return scrollContent.implicitHeight > actionsFlickable.height;
            }
        }
        readonly property real reservedScrollbarWidth: showScrollbar
            ? Math.max(actionsScrollbar.width, actionsScrollbar.implicitWidth) + Komai.paddingSmall
            : 0

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: Math.min(scrollContent.implicitHeight, root.parent ? root.parent.height * 0.85 : 600)
        clip: true

        Flickable {
            id: actionsFlickable

            anchors.fill: parent
            anchors.rightMargin: actionsScrollArea.reservedScrollbarWidth
            contentWidth: width
            contentHeight: scrollContent.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            ColumnLayout {
                id: scrollContent
                width: actionsFlickable.width
                spacing: Komai.paddingSmall

                // Message preview
                Reply {
                    id: replyPreview

                    Layout.fillWidth: true
                    Layout.maximumHeight: Math.min(root.parent ? root.parent.height * 0.4 : 300, 300)
                    clip: true
                    enabled: false
                    eventId: root.eventId
                    room_: root.hasRoleDataSource ? root.effectiveRoomModel : null
                    previewData: root.hasRoleDataSource ? ({}) : root.previewMessageData
                    roomModelOverride: root.hasRoleDataSource ? null : root.effectiveRoomModel
                    maxWidth: actionsFlickable.width

                    property bool isReplyFromCurrentUser: {
                        const currentUser = Komai.currentUser;
                        const currentUserId = (currentUser && currentUser.userid)
                                ? String(currentUser.userid)
                                : "";
                        return currentUserId.length > 0 && replyPreview.userId === currentUserId;
                    }
                    readonly property color previewWindowColor: (Komai.colors && Komai.colors.window !== undefined)
                        ? Komai.colors.window
                        : root.palette.window
                    readonly property color previewBaseColor: (Komai.colors && Komai.colors.base !== undefined)
                        ? Komai.colors.base
                        : root.palette.base
                    bubblePalette: root.effectiveRoomModel ? TimelineManager.roomUserBubblePalette(root.effectiveRoomModel.roomId, replyPreview.userId, roomColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userBubblePalette(replyPreview.userId, roomColor)
                    userColor: isReplyFromCurrentUser
                        ? Komai.theme.userColorSelf
                        : root.effectiveRoomModel ? TimelineManager.roomUserColor(root.effectiveRoomModel.roomId, replyPreview.userId, previewWindowColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyPreview.userId, previewWindowColor)
                    roomColor: isReplyFromCurrentUser
                        ? Komai.theme.userColorSelf
                        : root.effectiveRoomModel ? TimelineManager.roomUserColor(root.effectiveRoomModel.roomId, replyPreview.userId, previewBaseColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyPreview.userId, previewBaseColor)

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
                    shortcutSequence: "C"
                    shortcutDisplayText: qsTr("C")
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
                    shortcutSequence: "H"
                    shortcutDisplayText: qsTr("H")
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
                    shortcutSequence: "C"
                    shortcutDisplayText: qsTr("C")
                    visible: root.isMediaType && root.effectiveRoomModel && typeof root.effectiveRoomModel.copyMedia === "function"
                    onClicked: {
                        root.effectiveRoomModel.copyMedia(root.eventId);
                        showFeedback(qsTr("Copied!"));
                    }
                }

            ActionButton {
                id: copyLinkLocationBtn
                labelText: qsTr("Copy link location")
                iconSource: ":/icons/icons/ui/copy.svg"
                shortcutSequence: "L"
                shortcutDisplayText: qsTr("L")
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
                shortcutSequence: "K"
                shortcutDisplayText: qsTr("K")
                visible: root.eventId !== "" && !root.isStateEvent && root.effectiveRoomModel && typeof root.effectiveRoomModel.copyLinkToEvent === "function"
                onClicked: {
                    root.effectiveRoomModel.copyLinkToEvent(root.eventId);
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
                shortcutSequence: "P"
                shortcutDisplayText: qsTr("P")
                visible: root.canChangePinned && !root.isStateEvent
                    && root.effectiveRoomModel
                    && typeof root.effectiveRoomModel.pin === "function"
                    && typeof root.effectiveRoomModel.unpin === "function"
                onClicked: {
                    if (root.isPinned)
                        root.effectiveRoomModel.unpin(root.eventId);
                    else
                        root.effectiveRoomModel.pin(root.eventId);
                    showFeedback(root.isPinned ? qsTr("Unpinned!") : qsTr("Pinned!"));
                }
            }

            ActionButton {
                id: markReadBtn
                labelText: qsTr("Mark as read")
                iconSource: ":/icons/icons/ui/double-checkmark.svg"
                shortcutSequence: "M"
                shortcutDisplayText: qsTr("M")
                visible: !root.isStateEvent && root.effectiveRoomModel && typeof root.effectiveRoomModel.markEventAsRead === "function"
                onClicked: {
                    root.effectiveRoomModel.markEventAsRead(root.eventId);
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
                shortcutSequence: "S"
                shortcutDisplayText: qsTr("S")
                visible: root.isMediaType && root.effectiveRoomModel && typeof root.effectiveRoomModel.saveMedia === "function"
                onClicked: {
                    root.close();
                    root.effectiveRoomModel.saveMedia(root.eventId);
                }
            }

            ActionButton {
                id: openExternalBtn
                labelText: qsTr("Open in external program")
                iconSource: ":/icons/icons/ui/open-externally.svg"
                shortcutSequence: "O"
                shortcutDisplayText: qsTr("O")
                visible: root.isMediaType && root.effectiveRoomModel && typeof root.effectiveRoomModel.openMedia === "function"
                onClicked: {
                    root.close();
                    root.effectiveRoomModel.openMedia(root.eventId);
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
                shortcutSequence: "I"
                shortcutDisplayText: qsTr("I")
                visible: !root.isStateEvent && root.effectiveRoomModel && typeof root.effectiveRoomModel.showReadReceipts === "function"
                onClicked: {
                    root.close();
                    root.effectiveRoomModel.showReadReceipts(root.eventId);
                }
            }

            ActionButton {
                id: viewRawBtn
                labelText: qsTr("View raw message")
                iconSource: ":/icons/icons/ui/raw-message.svg"
                shortcutSequence: "U"
                shortcutDisplayText: qsTr("U")
                visible: root.effectiveRoomModel && typeof root.effectiveRoomModel.viewRawMessage === "function"
                onClicked: {
                    root.close();
                    root.effectiveRoomModel.viewRawMessage(root.eventId);
                }
            }

            ActionButton {
                id: viewDecryptedRawBtn
                labelText: qsTr("View decrypted raw message")
                iconSource: ":/icons/icons/ui/raw-message.svg"
                shortcutSequence: "E"
                shortcutDisplayText: qsTr("E")
                visible: root.isEncrypted && root.effectiveRoomModel && typeof root.effectiveRoomModel.viewDecryptedRawMessage === "function"
                onClicked: {
                    root.close();
                    root.effectiveRoomModel.viewDecryptedRawMessage(root.eventId);
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
                labelText: qsTr("Delete message")
                iconSource: ":/icons/icons/ui/delete.svg"
                shortcutSequence: "D"
                shortcutDisplayText: qsTr("D")
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
                    shortcutSequence: "R"
                    shortcutDisplayText: qsTr("R")
                    visible: !root.isStateEvent
                    onClicked: {
                        root.close();
                        root.chatRoot.openReportMessageDialog(root.eventId);
                    }
                }
            }
        }

        ScrollBar {
            id: actionsScrollbar

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            orientation: Qt.Vertical
            z: 1
            policy: ScrollBar.AlwaysOn
            visible: actionsScrollArea.showScrollbar
            opacity: actionsScrollArea.showScrollbar ? 1 : 0

            Binding on position {
                when: !actionsScrollbar.pressed
                value: actionsFlickable.contentHeight > 0 ? actionsFlickable.visibleArea.yPosition : 0
            }

            size: actionsFlickable.contentHeight > 0 ? actionsFlickable.visibleArea.heightRatio : 1

            onPositionChanged: {
                if (pressed)
                    actionsFlickable.contentY = position * Math.max(0, actionsFlickable.contentHeight - actionsFlickable.height);
            }
        }
    }

    Shortcut {
        enabled: root.visible
        sequence: "Up"
        context: Qt.WindowShortcut

        onActivated: root.moveActionFocus(-1)
    }
    Shortcut {
        enabled: root.visible
        sequence: "Down"
        context: Qt.WindowShortcut

        onActivated: root.moveActionFocus(1)
    }
}
