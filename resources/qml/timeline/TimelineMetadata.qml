// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

RowLayout {
    id: metadata
    property var contentPalette: null
    readonly property int colorRevision: TimelineManager.colorRevision
    readonly property bool hasContentPalette: contentPalette !== null && contentPalette !== undefined
    readonly property color effectiveBaseColor: {
        const _revision = colorRevision;
        if (hasContentPalette && contentPalette.base !== undefined && contentPalette.base !== null)
            return contentPalette.base;
        if (Komai.colors && Komai.colors.base !== undefined)
            return Komai.colors.base;
        return palette.base;
    }
    readonly property color effectiveTextColor: {
        const _revision = colorRevision;
        if (hasContentPalette && contentPalette.text !== undefined && contentPalette.text !== null)
            return contentPalette.text;
        if (Komai.colors && Komai.colors.text !== undefined)
            return Komai.colors.text;
        return palette.text;
    }
    readonly property color effectiveSecondaryTextColor: {
        const _revision = colorRevision;
        if (hasContentPalette && contentPalette.buttonText !== undefined && contentPalette.buttonText !== null)
            return contentPalette.buttonText;
        if (Komai.colors && Komai.colors.buttonText !== undefined)
            return Komai.colors.buttonText;
        return palette.buttonText;
    }
    readonly property color effectiveInactiveTextColor: {
        const _revision = colorRevision;
        if (hasContentPalette)
            return effectiveSecondaryTextColor;
        if (Komai.inactiveColors && Komai.inactiveColors.text !== undefined)
            return Komai.inactiveColors.text;
        return palette.inactive.text;
    }
    readonly property color effectiveHighlightColor: {
        const _revision = colorRevision;
        if (hasContentPalette && contentPalette.highlight !== undefined && contentPalette.highlight !== null)
            return contentPalette.highlight;
        if (Komai.colors && Komai.colors.highlight !== undefined)
            return Komai.colors.highlight;
        return palette.highlight;
    }

    property int iconSize: Math.floor(fontMetrics.ascent * scaling)
    property int rawButtonSize: Math.round(iconSize * buttonScale)
    property int buttonSize: (rawButtonSize % 2 === 0) ? rawButtonSize : (rawButtonSize + 1)
    property int rawIndicatorSize: Math.round(iconSize * 1.5)
    property int indicatorSize: (rawIndicatorSize % 2 === 0) ? rawIndicatorSize : (rawIndicatorSize + 1)
    required property double scaling
    property double buttonScale: 2
    required property bool isSender
    property bool actionBarActive: false
    property bool actionsEnabled: true
    readonly property var actionToggleButton: actionToggle

    signal actionToggled()
    signal readReceiptsRequested(string eventId)

    LayoutMirroring.enabled: false
    layoutDirection: metadata.isSender ? Qt.RightToLeft : Qt.LeftToRight

    required property string eventId
    property bool isLocalEcho: false
    property string transactionId: ""
    // Human-readable SDK error for failed local echoes. Empty otherwise.
    property string sendError: ""
    // True when matrix-sdk marked the failure as transient (retry-worthy).
    // Meaningful only when `status == MtxEvent.Failed`.
    property bool isRecoverable: false
    required property int status
    required property int trustlevel
    // Richer per-message shield code (Crypto.MessageShield). Default is
    // ShieldNone = 0 so callers that don't plumb a value yet get the
    // "verified/no warning" rendering, consistent with the pre-shield behaviour.
    property int messageShield: Crypto.ShieldNone
    // Item kind from the Rust backend ("message", "unable_to_decrypt", …).
    // Used to suppress the shield for UTDs — matrix-sdk returns no shield
    // for them, and the Encrypted delegate already explains the status.
    property string typeString: ""
    required property bool isEdited
    required property bool isEncrypted
    required property bool isStateEvent
    required property string threadId
    property bool isThreadRoot: false
    property int threadReplyCount: 0
    required property date timestamp
    required property var room
    readonly property string roomEditEventId: (room && room.edit !== undefined) ? room.edit : ""
    readonly property bool roomIsEncrypted: (room && room.isEncrypted !== undefined) ? room.isEncrypted : false
    readonly property string effectiveThreadId: threadId || (isThreadRoot ? eventId : "")
    readonly property bool canOpenThreadNavigation: !!room
        && effectiveThreadId !== ""
        && room.supportsThreadNavigation !== false
        && room.thread !== undefined

    // `Settings.resolvedTimelineThreadsCollapseReplies(roomId)` is a
    // Q_INVOKABLE that won't re-evaluate on settings changes by itself,
    // so we bump a revision counter from the Settings change signals and
    // depend on it inside the binding (mirrors the pattern in
    // MatrixRoomView.qml).
    property int _collapseRepliesRevision: 0
    readonly property bool collapseThreadRepliesActive: {
        const _rev = _collapseRepliesRevision;
        const roomId = room ? String(room.roomId || "") : "";
        if (roomId.length === 0)
            return Settings.timelineThreadsCollapseReplies;
        return Settings.resolvedTimelineThreadsCollapseReplies(roomId);
    }
    Connections {
        target: Settings
        function onTimelineThreadsCollapseRepliesChanged() {
            metadata._collapseRepliesRevision++;
        }
        function onTimelineThreadsCollapseRepliesByRoomChanged() {
            metadata._collapseRepliesRevision++;
        }
    }

    spacing: 2

    Label {
        id: ts

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredWidth: implicitWidth
        color: effectiveInactiveTextColor
        font.pointSize: Settings.uiFontSizePt * parent.scaling
        text: metadata.timestamp.toLocaleTimeString(Locale.ShortFormat)

        HoverHandler {
            id: ma

        }

        KomaiToolTip {
            anchorItem: ts
            anchorX: ts.width / 2
            anchorY: 0
            text: Qt.formatDateTime(metadata.timestamp, Qt.DefaultLocaleLongDate)
            delay: Komai.tooltipDelay
            requestedVisible: ma.hovered
        }
    }

    StatusIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        visible: !metadata.isStateEvent && metadata.status != MtxEvent.Empty
        eventId: metadata.eventId
        status: metadata.status
        sendError: metadata.sendError
        onReadReceiptsRequested: (eventId) => metadata.readReceiptsRequested(eventId)
    }
    // Retry button for wedged-but-recoverable local echoes. A single click
    // calls matrix-sdk's `SendHandle::unwedge()` which re-queues the event.
    // Hidden for non-recoverable failures (e.g. server-rejected content) —
    // unwedging those would just re-fail; users should cancel instead.
    ImageButton {
        id: retryButton

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.indicatorSize
        Layout.preferredWidth: parent.indicatorSize
        visible: metadata.status === MtxEvent.Failed
            && metadata.isLocalEcho
            && metadata.isRecoverable
            && metadata.transactionId.length > 0
        image: ":/icons/icons/ui/refresh.svg"
        toolTipText: qsTr("Retry sending")
        toolTipVisible: hovered
        buttonTextColor: palette.buttonText
        highlightColor: Komai.theme.error
        changeColorOnHover: true
        cursor: Qt.PointingHandCursor
        onClicked: TimelineManager.retryActiveMatrixTimelineLocalEcho(metadata.transactionId)
    }
    Image {
        id: editedMarker

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: metadata.indicatorSize
        Layout.preferredWidth: metadata.indicatorSize
        // Local echoes never reached the server, so they can't be "edited" and
        // can't be the composer's active edit target. The lookup-key `eventId`
        // may coincide with a stale `room.edit` value in surprising ways — gate
        // explicitly on `!isLocalEcho` plus a non-empty match.
        visible: !metadata.isLocalEcho
            && (metadata.isEdited
                || (metadata.eventId !== "" && metadata.eventId === metadata.roomEditEventId))
        source: visible ? "image://colorimage/:/icons/icons/ui/edit.svg?" + ((metadata.eventId !== "" && metadata.eventId === metadata.roomEditEventId) ? effectiveHighlightColor : effectiveSecondaryTextColor) : ""
        sourceSize.height: metadata.indicatorSize
        sourceSize.width: metadata.indicatorSize
        HoverHandler {
            id: editHovered

        }

        KomaiToolTip {
            anchorItem: editedMarker
            anchorX: editedMarker.width / 2
            anchorY: 0
            text: qsTr("Edited")
            delay: Komai.tooltipDelay
            requestedVisible: editHovered.hovered
        }
    }
    EncryptionIndicator {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: parent.buttonSize
        Layout.preferredWidth: parent.buttonSize
        encrypted: metadata.isEncrypted
        sourceSize.height: parent.buttonSize
        sourceSize.width: parent.buttonSize
        trust: metadata.trustlevel
        // Drive the indicator off the richer per-message shield code; the
        // legacy `trust` bucket is kept as a fallback for UI surfaces that
        // haven't been migrated (room header, member list).
        shield: metadata.messageShield
        useShield: true
        // Matrix state events (m.room.create, m.room.encryption, membership,
        // topic, …) are sent in the clear by spec — flagging them as
        // unencrypted next to every state event is just noise. UTDs are
        // handled by the Encrypted delegate itself, and matrix-sdk's
        // `get_shield()` returns None for them anyway, so skip the shield.
        // Local echoes report `isEncrypted=false` until matrix-sdk populates
        // `encryption_info()` on the sent event — rendering the indicator
        // during that window flashes a red "not encrypted" shield for a
        // message that's about to be encrypted, so hide it until the remote
        // echo lands and the real shield code is available.
        visible: metadata.roomIsEncrypted
            && !metadata.isStateEvent
            && metadata.typeString !== "unable_to_decrypt"
            && !metadata.isLocalEcho
    }
    ImageButton {
        id: unpinButton

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: metadata.buttonSize
        Layout.preferredWidth: metadata.buttonSize
        visible: {
            const pinnedIds = TimelineManager.matrixTimelinePinnedEventIds;
            return pinnedIds && pinnedIds.indexOf(metadata.eventId) >= 0;
        }
        toolTipText: qsTr("Unpin")
        toolTipVisible: hovered
        buttonTextColor: effectiveSecondaryTextColor
        highlightColor: effectiveHighlightColor
        changeColorOnHover: true
        image: visible ? ":/icons/icons/ui/pin-off.svg" : ""

        onClicked: TimelineManager.unpinActiveMatrixTimelineEvent(metadata.eventId)
    }
    ImageButton {
        id: actionToggle

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredHeight: metadata.buttonSize
        Layout.preferredWidth: metadata.buttonSize
        visible: metadata.actionsEnabled && Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton
        toolTipDelay: 0
        toolTipText: qsTr("Message actions")
        toolTipVisible: hovered && !metadata.actionBarActive
        buttonTextColor: metadata.actionBarActive ? effectiveHighlightColor : Qt.rgba(effectiveInactiveTextColor.r, effectiveInactiveTextColor.g, effectiveInactiveTextColor.b, 0.35)
        highlightColor: effectiveHighlightColor
        changeColorOnHover: true
        image: visible ? ":/icons/icons/ui/textbox-more.svg" : ""

        onClicked: metadata.actionToggled()
    }
    Item {
        id: threadButtonContainer

        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        Layout.preferredWidth: threadIcon.width + (threadReplyCountLabel.visible ? threadReplyCountLabel.implicitWidth + 1 : 0)
        Layout.preferredHeight: metadata.buttonSize
        visible: metadata.canOpenThreadNavigation

        readonly property color threadColor: {
            const _revision = colorRevision;
            return TimelineManager.userColor(metadata.effectiveThreadId, effectiveBaseColor);
        }

        Image {
            id: threadIcon
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: metadata.buttonSize
            height: metadata.buttonSize
            sourceSize.width: width
            sourceSize.height: height
            source: threadButtonContainer.visible
                ? "image://colorimage/:/icons/icons/ui/thread.svg?" + threadButtonContainer.threadColor
                : ""
        }
        Label {
            id: threadReplyCountLabel
            anchors.left: threadIcon.right
            anchors.leftMargin: 1
            anchors.verticalCenter: parent.verticalCenter
            // Only meaningful when replies are hidden — otherwise the
            // thread's replies are already inline in the timeline and the
            // count is just visual noise next to the root's thread icon.
            visible: metadata.isThreadRoot
                && metadata.threadReplyCount > 0
                && metadata.collapseThreadRepliesActive
            text: metadata.threadReplyCount.toLocaleString()
            color: threadButtonContainer.threadColor
            font.pointSize: Settings.uiFontSizePt * metadata.scaling * 1.2
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            onClicked: {
                if (metadata.room)
                    metadata.room.thread = metadata.effectiveThreadId;
            }

            HoverHandler {
                id: threadHover
            }
        }

        KomaiToolTip {
            anchorItem: threadButtonContainer
            anchorX: threadButtonContainer.width / 2
            anchorY: 0
            text: metadata.isThreadRoot && metadata.threadReplyCount > 0
                ? qsTr("%n thread reply(s)", "", metadata.threadReplyCount)
                : qsTr("Reply in this thread")
            delay: Komai.tooltipDelay
            requestedVisible: threadHover.hovered
        }
    }
}
