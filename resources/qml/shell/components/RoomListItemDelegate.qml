// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "." as ShellComponents
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ItemDelegate {
    id: roomItem

    required property int density
    required property int avatarSize
    required property bool collapsed
    readonly property real baseFontPixelSize: Komai.fontPixelSize
    required property var roomContextMenu
    required property real scrollbarReservedWidth
    required property var tabController
    required property string avatarUrl
    property color backgroundColor: palette.window
    property color bubbleBackground: palette.highlight
    property color bubbleText: palette.highlightedText
    required property string directChatOtherUserId
    required property bool hasLoudNotification
    required property bool hasUnreadMessages
    required property bool hasDraft
    property color draftIndicatorColor: Komai.theme.attention
    property color unreadIndicatorColor: palette.highlight
    property color importantText: palette.text
    required property bool isDirect
    required property bool isInvite
    required property bool isSpace
    required property string lastMessage
    required property string lastMessagePreviewSenderName
    required property string lastMessagePreviewBody
    required property string draftPreview
    required property int unreadCount
    required property string roomId
    required property string roomName
    required property var tags
    required property string time
    required property bool isEncrypted
    readonly property bool isSelected: roomId === Rooms.currentRoomId
    readonly property bool isLowPriorityRoom: !!tags && tags.indexOf && tags.indexOf("m.lowpriority") !== -1
    // emphasizeUnreadState governs every visual signal tied to unread state
    // (bold title, avatar bounce, row highlight, left-edge marker, bubble),
    // so the global toggle lives here; drafts stay emphasized regardless.
    // _scrubMarkedAsRead is the optimistic flag set when the scrub gesture
    // fires: gating emphasis on it makes the unread visuals disappear the
    // moment the gesture is recognised, instead of after the read-receipt
    // round-trip echoes back through sliding sync.
    readonly property bool emphasizeUnreadState: hasUnreadMessages && !_scrubMarkedAsRead && (!isLowPriorityRoom || hasLoudNotification || Communities.currentFilterId === "tag:m.lowpriority") && Settings.navigationRoomListShowUnreadIndicators
    readonly property bool emphasizeDraftState: hasDraft && !emphasizeUnreadState
    readonly property bool emphasizeActivityState: emphasizeUnreadState || emphasizeDraftState
    readonly property bool keyboardFocused: ListView.view && ListView.view.activeFocus && ListView.isCurrentItem
    // Mouse-scrub-to-mark-read state. scrubProgress (0..1) drives the row's
    // visual feedback during the gesture; _scrubFiredThisGesture suppresses
    // the would-be click that follows on release; _scrubMarkedAsRead is the
    // optimistic "treat this row as read" flag — it stays set until the
    // delegate gets rebound to a different room (delegate reuse on scroll).
    property real scrubProgress: 0
    property bool _scrubFiredThisGesture: false
    property bool _scrubMarkedAsRead: false
    onRoomIdChanged: _scrubMarkedAsRead = false
    readonly property color draftActivityBase: Qt.rgba((Komai.theme.attention.r + palette.highlight.r) / 2, (Komai.theme.attention.g + palette.highlight.g) / 2, (Komai.theme.attention.b + palette.highlight.b) / 2, 1)
    readonly property color hoverBackground: Qt.rgba(palette.dark.r * 0.30 + palette.window.r * 0.70, palette.dark.g * 0.30 + palette.window.g * 0.70, palette.dark.b * 0.30 + palette.window.b * 0.70, 1)
    readonly property color selectedBackground: Qt.rgba(palette.dark.r * 0.85 + palette.window.r * 0.15, palette.dark.g * 0.85 + palette.window.g * 0.15, palette.dark.b * 0.85 + palette.window.b * 0.15, 1)
    // Second-line text is derived from palette.text via MutedText, which
    // also owns the WCAG-tuned blend factors shared with the bubble metadata
    // timestamp in TimelineMetadata. See components/MutedText.qml for the
    // rationale (and update the factors there, not here).
    property color previewText: MutedText.muted(palette.text, palette.window, MutedText.previewBlend)
    property color timestampText: MutedText.muted(palette.text, palette.window, MutedText.timestampBlend)

    KomaiToolTip {
        anchorItem: roomItem
        anchorX: roomItem.width / 2
        anchorY: roomItem.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: roomItem.roomName
        delay: Komai.tooltipDelay
        requestedVisible: roomItem.hovered && roomItem.collapsed
    }

    height: Komai.navigationRowHeight
    state: "normal"
    width: ListView.view.width - scrollbarReservedWidth

    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    activeFocusOnTab: false
    focusPolicy: Qt.NoFocus

    Accessible.role: Accessible.ListItem
    Accessible.name: {
        if (isInvite)
            return qsTr("Invite: %1").arg(roomName);
        if (isSpace)
            return qsTr("Space: %1").arg(roomName);
        return roomName;
    }
    Accessible.checkable: true
    Accessible.checked: isSelected
    // Aggregate state visible to sighted users (unread count, draft,
    // encrypted, last message preview, time) so screen readers announce
    // the same at-a-glance signal in one shot. The inner avatar and text
    // columns are marked ignored below since the info is already here.
    Accessible.description: {
        var parts = [];
        if (emphasizeUnreadState && unreadCount > 0)
            parts.push(qsTr("%n unread message(s)", "", unreadCount));
        if (emphasizeUnreadState && hasLoudNotification)
            parts.push(qsTr("Mentions you"));
        if (hasDraft)
            parts.push(qsTr("Has draft"));
        if (isEncrypted)
            parts.push(qsTr("Encrypted"));
        if (lastMessage.length > 0)
            parts.push(lastMessage);
        if (time.length > 0)
            parts.push(time);
        return parts.join(". ");
    }

    background: Rectangle {
        color: backgroundColor

        Rectangle {
            anchors.fill: parent
            color: roomItem.emphasizeDraftState
                ? Qt.rgba(Komai.theme.attention.r, Komai.theme.attention.g, Komai.theme.attention.b, 0.12)
                : Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.15)
            visible: roomItem.emphasizeActivityState && roomItem.state !== "selected"
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: palette.highlight
            border.width: roomItem.keyboardFocused ? 2 : 0
        }
    }
    states: [
        State {
            name: "highlight"
            when: roomItem.hovered && !roomItem.isSelected

            PropertyChanges {
                roomItem {
                    backgroundColor: roomItem.hoverBackground
                    bubbleBackground: palette.highlight
                    bubbleText: palette.highlightedText
                    importantText: palette.text
                    previewText: palette.text
                    timestampText: palette.text
                }
            }
        },
        State {
            name: "selected"
            when: roomItem.isSelected

            PropertyChanges {
                roomItem {
                    backgroundColor: roomItem.selectedBackground
                    bubbleBackground: palette.highlight
                    bubbleText: palette.highlightedText
                    draftIndicatorColor: palette.brightText
                    importantText: palette.brightText
                    previewText: palette.brightText
                    timestampText: palette.brightText
                    unreadIndicatorColor: palette.brightText
                }
            }
        }
    ]

    onClicked: {} // Click logic handled by roomClickArea below for modifier detection.
    onPressAndHold: {
        if (_scrubFiredThisGesture)
            return; // a scrub gesture already consumed this press
        if (!isInvite)
            roomContextMenu.show(roomItem, roomId, tags, isSpace, isInvite, hasUnreadMessages || hasLoudNotification);
    }

    // Keep 1px of padding here so the touch areas do not overlap.
    Item {
        anchors.fill: parent
        anchors.margins: 1

        TapHandler {
            id: roomItemTh

            acceptedButtons: Qt.RightButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: {
                if (!isInvite)
                    roomContextMenu.show(roomItemTh.parent, roomId, tags, isSpace, isInvite, hasUnreadMessages || hasLoudNotification);
            }
        }
        MouseArea {
            id: roomClickArea

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
            hoverEnabled: true
            cursorShape: roomItem.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            // Without this, the parent ListView's Flickable steals the press
            // ~120ms in for flick/scroll detection, killing the scrub gesture
            // mid-drag (we'd see onCanceled fire). Holding the press here
            // keeps the gesture intact; we don't drag the row anywhere, so
            // we're not contending with any real flick semantics.
            preventStealing: true

            // Scrub-to-mark-read tracking. We sample horizontal motion while
            // the left button is held: a gesture fires once it accumulates
            // ≥3 direction reversals AND ≥80px of cumulative travel within
            // a 1.5s rolling window. The distance bar is small on purpose
            // — high-poll-rate mice produce lots of 1px deltas during a
            // quick shake, so 80px stays achievable; the reversal count is
            // what carries the "this is intentional" signal.
            property real scrubDistance: 0
            property int scrubReversals: 0
            property real scrubLastX: 0
            property int scrubLastDxSign: 0
            property real scrubStartedAt: 0

            function _resetScrubTracking() {
                scrubDistance = 0;
                scrubReversals = 0;
                scrubLastX = 0;
                scrubLastDxSign = 0;
                scrubStartedAt = 0;
            }

            function _scrubEligible() {
                if (roomItem.isSpace || roomItem.isInvite)
                    return false;
                return roomItem.hasUnreadMessages || roomItem.hasLoudNotification;
            }

            onPressed: function(mouse) {
                if (mouse.button !== Qt.LeftButton)
                    return;
                roomItem._scrubFiredThisGesture = false;
                scrubResetAnim.stop();
                roomItem.scrubProgress = 0;
                _resetScrubTracking();
                if (_scrubEligible()) {
                    scrubLastX = mouse.x;
                    scrubStartedAt = Date.now();
                }
            }
            onPositionChanged: function(mouse) {
                if (!pressed || roomItem._scrubFiredThisGesture)
                    return;
                if (!(mouse.buttons & Qt.LeftButton))
                    return;
                if (!_scrubEligible())
                    return;

                var now = Date.now();
                if (scrubStartedAt === 0) {
                    scrubStartedAt = now;
                    scrubLastX = mouse.x;
                    return;
                }
                if (now - scrubStartedAt > 1500) {
                    // Window elapsed without firing — restart from current position.
                    scrubStartedAt = now;
                    scrubDistance = 0;
                    scrubReversals = 0;
                    scrubLastDxSign = 0;
                    scrubLastX = mouse.x;
                    roomItem.scrubProgress = 0;
                    return;
                }

                var dx = mouse.x - scrubLastX;
                scrubLastX = mouse.x;
                if (dx === 0)
                    return; // no motion this event

                scrubDistance += Math.abs(dx);
                var sign = dx > 0 ? 1 : -1;
                if (scrubLastDxSign !== 0 && sign !== scrubLastDxSign)
                    scrubReversals += 1;
                scrubLastDxSign = sign;

                var distP = Math.min(1, scrubDistance / 80);
                var revP = Math.min(1, scrubReversals / 3);
                roomItem.scrubProgress = Math.min(distP, revP);

                if (scrubReversals >= 3 && scrubDistance >= 80) {
                    roomItem._scrubFiredThisGesture = true;
                    roomItem._scrubMarkedAsRead = true;
                    roomItem.scrubProgress = 1.0;
                    Rooms.markAsRead(roomItem.roomId);
                    scrubResetAnim.restart();
                }
            }
            onReleased: function(mouse) {
                _resetScrubTracking();
                if (!roomItem._scrubFiredThisGesture && roomItem.scrubProgress > 0)
                    scrubResetAnim.restart();
            }
            onCanceled: {
                _resetScrubTracking();
                if (!roomItem._scrubFiredThisGesture && roomItem.scrubProgress > 0)
                    scrubResetAnim.restart();
            }

            onClicked: function(mouse) {
                if (roomItem._scrubFiredThisGesture) {
                    // Scrub already did the job — don't switch rooms on top.
                    return;
                }
                if (mouse.button === Qt.MiddleButton) {
                    // Middle-click opens a tab; for invites we surface the
                    // accept/decline dialog instead, matching left-click.
                    if (isInvite)
                        TimelineManager.openInviteResponseDialog(roomId);
                    else
                        tabController.openTab(roomId);
                    return;
                }
                var ctrlHeld = !!(mouse.modifiers & Qt.ControlModifier);
                tabController.handleRoomClick(roomId, isInvite, ctrlHeld);
            }
            onPressAndHold: function(mouse) {
                if (roomItem._scrubFiredThisGesture)
                    return; // a scrub gesture already consumed this press
                if (!isInvite)
                    roomContextMenu.show(roomItem, roomId, tags, isSpace, isInvite, hasUnreadMessages || hasLoudNotification);
            }
        }
        NumberAnimation {
            id: scrubResetAnim
            target: roomItem
            property: "scrubProgress"
            to: 0
            duration: 250
            easing.type: Easing.OutCubic
        }
    }
    RowLayout {
        id: mainContent

        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium + Komai.paddingSmall
        anchors.rightMargin: Komai.paddingMedium + Komai.paddingSmall
        anchors.topMargin: density === Settings.Density.Spacious ? Komai.paddingMedium : Komai.paddingSmall / 2
        anchors.bottomMargin: density === Settings.Density.Spacious ? Komai.paddingMedium : Komai.paddingSmall / 2
        spacing: Komai.paddingMedium
        opacity: 1.0 - 0.6 * roomItem.scrubProgress

        ShellComponents.RoomListItemAvatar {
            id: avatar

            avatarSize: roomItem.avatarSize
            roomName: roomItem.roomName
            roomId: roomItem.roomId
            avatarUrl: roomItem.avatarUrl
            isDirect: roomItem.isDirect
            directChatOtherUserId: roomItem.directChatOtherUserId
            bubbleBackground: roomItem.bubbleBackground
            bubbleText: roomItem.bubbleText
            hasLoudNotification: roomItem.hasLoudNotification
            hasUnreadMessages: roomItem.emphasizeUnreadState
            collapsed: roomItem.collapsed
            isSpace: roomItem.isSpace
            unreadCount: roomItem.unreadCount
            bounceOnUnread: roomItem.emphasizeUnreadState
            isSelected: roomItem.isSelected
            Accessible.ignored: true
        }
        ShellComponents.RoomListItemTextContent {
            density: roomItem.density
            collapsed: roomItem.collapsed
            isSpace: roomItem.isSpace
            isInvite: roomItem.isInvite
            isEncrypted: roomItem.isEncrypted
            hasUnreadMessages: roomItem.emphasizeUnreadState
            hasDraft: roomItem.hasDraft
            hasLoudNotification: roomItem.hasLoudNotification
            unreadCount: roomItem.unreadCount
            avatarHeight: avatar.height
            baseFontPixelSize: roomItem.baseFontPixelSize
            roomName: roomItem.roomName
            lastMessage: roomItem.lastMessage
            lastMessagePreviewSenderName: roomItem.lastMessagePreviewSenderName
            lastMessagePreviewBody: roomItem.lastMessagePreviewBody
            draftPreview: roomItem.draftPreview
            time: roomItem.time
            draftIndicatorColor: roomItem.draftIndicatorColor
            importantText: roomItem.importantText
            previewText: roomItem.previewText
            timestampText: roomItem.timestampText
            bubbleBackground: roomItem.bubbleBackground
            bubbleText: roomItem.bubbleText
            Accessible.ignored: true
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Komai.theme.separator
        height: 1
        Accessible.ignored: true
    }
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Komai.paddingSmall / 2
        anchors.verticalCenter: parent.verticalCenter
        color: roomItem.emphasizeDraftState ? Komai.theme.attention : roomItem.unreadIndicatorColor
        height: parent.height - Komai.paddingMedium * 2
        visible: roomItem.emphasizeActivityState
        width: 6
        radius: 3
        opacity: 1.0 - roomItem.scrubProgress
        Accessible.ignored: true
    }
}
