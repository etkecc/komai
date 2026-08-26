// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../delegates/"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai 1.0

Popup {
    id: forwardMessagePopup

    property string mid: ""
    property var messageEventIds: []
    property int selectionCount: 0
    property var roomSource: null
    property real dialogViewportWidth: 0
    readonly property var activeRoom: roomSource
    property var timelineSource: null
    property var timelineViewSource: null
    readonly property var timeline: timelineSource
    readonly property var timelineView: timelineViewSource
    property bool showReplyPreview: true
    property color modalOverlayColor: Qt.rgba(0, 0, 0, palette.window.hslLightness < 0.5 ? 0.76 : 0.68)
    property int textHeight: Math.round(Komai.fontPixelSize * 2.4)
    property int textMargin: Komai.paddingSmall
    property real anchoredY: -1
    // How far Ctrl+D / Ctrl+U jump through the suggestion list.
    readonly property int pageStep: 5
    readonly property int messageCount: messageEventIds.length
    readonly property int effectiveSelectionCount: Math.max(selectionCount, messageCount)
    readonly property int recipientCount: recipientsModel.count
    readonly property real innerWidth: width - leftPadding - rightPadding
    // parent is Overlay.overlay (fills the window). Popup is not an Item, so the
    // Window.window attached property is unavailable on the root.
    readonly property real windowHeight: parent ? parent.height : 800
    readonly property bool darkPopupChrome: palette.window.hslLightness < 0.5
    readonly property color popupOutlineColor: Qt.tint(
        palette.mid,
        Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, darkPopupChrome ? 0.22 : 0.32))

    // Card row metrics shared by the results and staging panels.
    readonly property int cardRowHeight: Komai.iconSize + Komai.paddingSmall * 2
    readonly property int listSpacing: Komai.paddingSmall

    // Scrollbar handling, honouring the user's tri-state scrollbar policy.
    readonly property int scrollbarPolicySetting: Settings.uiScrollbarPolicy
    // Reserve a gutter whenever a scrollbar can appear, so it never paints over
    // the rows and the row width stays stable when it shows/hides.
    readonly property bool scrollbarsReserveGutter: scrollbarPolicySetting !== Settings.ScrollbarPolicy.Never

    // Search backend for the destination rooms. Rebuilt from scratch on every
    // open so the dialog never carries state (or a stale room set) between uses.
    property var roomCompleter: null

    // Fixed reserved height for a list panel: a function of row count and the
    // window only — never of the content — so the panels keep a constant size and
    // nothing shifts under the pointer while picking rooms.
    function reservedListHeight(rows) {
        const rowH = cardRowHeight + listSpacing;
        const cap = Math.round(windowHeight * 0.34);
        return Math.max(rowH * 2, Math.min(rowH * rows, cap));
    }

    // Height budget consumed by the parts of the dialog that stay pinned
    // outside the scrollable middle section (title row, hint, and the
    // Forward/close action row), so the middle section knows how much room
    // it actually has left before it needs to start scrolling instead of
    // shoving the action row off the bottom of a short window.
    readonly property real scrollChromeHeight: topPadding + bottomPadding
        + forwardColumn.spacing * 3
        + titleRow.implicitHeight + hintLabel.implicitHeight + actionRow.implicitHeight
    readonly property real availableScrollHeight: Math.max(
        cardRowHeight * 2,
        windowHeight - Komai.paddingLarge * 2 - scrollChromeHeight)

    // Reserve the scrollbar gutter for the outer scroll area too, mirroring
    // the per-list gutter handling below.
    readonly property real outerScrollGutter: scrollbarsReserveGutter
        ? (forwardScrollBar.implicitWidth + Komai.paddingSmall)
        : 0
    readonly property real scrollContentWidth: innerWidth - outerScrollGutter

    function normalizedMessageEventIds(eventIdsIn) {
        const sourceIds = eventIdsIn || [];
        const normalizedIds = [];
        const seenIds = ({});

        for (let index = 0; index < sourceIds.length; index++) {
            const eventId = String(sourceIds[index] || "");
            if (!eventId || seenIds[eventId])
                continue;

            seenIds[eventId] = true;
            normalizedIds.push(eventId);
        }

        return normalizedIds;
    }

    function setMessageEventId(mid_in) {
        setMessageEventIds([mid_in], 1);
    }
    function setMessageEventIds(eventIdsIn, selectionCountIn) {
        messageEventIds = normalizedMessageEventIds(eventIdsIn);
        selectionCount = Math.max(Number(selectionCountIn || 0), messageEventIds.length);
        mid = messageEventIds.length > 0 ? String(messageEventIds[0] || "") : "";
    }

    function recipientIndexOf(roomId) {
        const id = String(roomId || "");
        for (let index = 0; index < recipientsModel.count; index++) {
            if (recipientsModel.get(index).roomId === id)
                return index;
        }
        return -1;
    }
    function containsRecipient(roomId) {
        return recipientIndexOf(roomId) >= 0;
    }
    function addRecipient(roomId) {
        const id = String(roomId || "");
        if (!id || containsRecipient(id))
            return false;

        const preview = Rooms.getRoomPreviewById(id);
        recipientsModel.append({
            "roomId": id,
            "roomName": preview ? preview.roomName : id,
            "avatarUrl": preview ? preview.roomAvatarUrl : "",
        });
        return true;
    }
    function removeRecipient(roomId) {
        const index = recipientIndexOf(roomId);
        if (index >= 0)
            recipientsModel.remove(index);
    }
    // Add the highlighted (or, if none, the first) suggestion to the recipients.
    // The search is intentionally NOT reset: keeping the rows in place lets the
    // user stage several in a row without re-aiming as the list reshuffles.
    function stageCurrentSuggestion() {
        if (suggestionList.count === 0)
            return;

        let index = suggestionList.currentIndex;
        if (index < 0 || index >= suggestionList.count)
            index = 0;

        const item = suggestionList.itemAtIndex(index);
        if (item && item.enabled)
            addRecipient(item.rawRoomId);
    }
    function resetSearch() {
        roomTextInput.text = "";
        if (roomCompleter)
            roomCompleter.searchString = "";
        suggestionList.currentIndex = suggestionList.count > 0 ? 0 : -1;
        roomTextInput.forceActiveFocus();
    }
    function forwardToRecipients() {
        if (activeRoom && recipientsModel.count > 0) {
            const eventIds = forwardMessagePopup.messageEventIds;
            for (let r = 0; r < recipientsModel.count; r++) {
                const targetRoomId = recipientsModel.get(r).roomId;
                if (eventIds.length > 1 && typeof activeRoom.forwardMessages === "function") {
                    activeRoom.forwardMessages(eventIds, targetRoomId);
                } else {
                    for (let index = 0; index < eventIds.length; index++)
                        activeRoom.forwardMessage(String(eventIds[index] || ""), targetRoomId);
                }
            }
        }
        forwardMessagePopup.close();
    }

    function titleText() {
        if (messageCount === 1 && effectiveSelectionCount <= 1)
            return qsTr("Forward message");
        if (effectiveSelectionCount > messageCount)
            return qsTr("Forward %1 of %2 messages").arg(messageCount).arg(effectiveSelectionCount);

        return qsTr("Forward %n messages", "", messageCount);
    }

    function hintText() {
        if (messageCount <= 1)
            return qsTr("Forwarding sends this content (without revealing its sender) to the rooms you pick below.");
        if (effectiveSelectionCount > messageCount) {
            if (messageCount === 1)
                return qsTr("Only 1 of %1 selected messages can be forwarded. Unsupported messages will be skipped.").arg(effectiveSelectionCount);
            return qsTr("Only %1 of %2 selected messages can be forwarded. Unsupported messages will be skipped.").arg(messageCount).arg(effectiveSelectionCount);
        }

        return qsTr("Forwarding sends these messages (without revealing their sender) to the rooms you pick below.");
    }

    function selectedHeadingText() {
        if (recipientCount > 0)
            return qsTr("Selected rooms (%1)").arg(recipientCount);
        return qsTr("Selected rooms");
    }

    padding: Komai.paddingMedium
    modal: true
    focus: true

    parent: Overlay.overlay
    width: ((dialogViewportWidth > 0)
                ? dialogViewportWidth
                : (parent ? parent.width : 900)) * 0.8
    x: Math.round(parent.width / 2 - width / 2)
    // anchoredY pins the top so the dialog doesn't jump under the pointer as
    // its content reflows (see onOpened below). But a value pinned while the
    // window was tall (e.g. opened maximized, then unmaximized/shrunk) must
    // not be allowed to push the dialog past the *current* window bounds —
    // re-clamp live against parent.height/height on every resize.
    y: {
        const desired = anchoredY >= 0
                ? anchoredY
                : Math.round((parent.height - height) / 2);
        const maxY = Math.max(Komai.paddingLarge, parent.height - height - Komai.paddingLarge);
        return Math.max(Komai.paddingLarge, Math.min(desired, maxY));
    }

    Overlay.modal: Rectangle {
        color: forwardMessagePopup.modalOverlayColor
    }
    background: Rectangle {
        color: palette.alternateBase
        radius: 8
        border.color: forwardMessagePopup.popupOutlineColor
        border.width: 2
    }

    // Ctrl+Enter sends to every selected room from anywhere in the dialog.
    Shortcut {
        sequences: ["Ctrl+Return", "Ctrl+Enter"]
        context: Qt.ApplicationShortcut
        enabled: forwardMessagePopup.visible && forwardMessagePopup.recipientCount > 0
        onActivated: forwardMessagePopup.forwardToRecipients()
    }

    ListModel {
        id: recipientsModel
    }

    onOpened: {
        // Full reset every open: no recipients, fresh completer, empty search.
        recipientsModel.clear();
        roomCompleter = TimelineManager.completerFor("forwardRoom", "");
        if (roomCompleter)
            roomCompleter.searchString = "";
        roomTextInput.text = "";
        suggestionList.currentIndex = suggestionList.count > 0 ? 0 : -1;
        // In image-overlay flow the closing overlay window can steal focus for a tick.
        Qt.callLater(() => {
            forwardMessagePopup.forceActiveFocus();
            roomTextInput.forceActiveFocus();
            // Pin the top of the dialog where it first lands.
            anchoredY = Math.max(Komai.paddingLarge,
                                 Math.round((parent.height - height) / 2));
        });
    }

    onClosed: {
        anchoredY = -1;
        recipientsModel.clear();
        roomCompleter = null;
    }

    // A plain Column (not ColumnLayout) is deliberate: the Reply preview embeds a
    // TimelineEvent whose height is width-driven, and a Layout's width negotiation
    // sends it into a polish() loop. Fixed widths keep it stable.
    contentItem: Column {
        id: forwardColumn

        spacing: Komai.paddingSmall

        // Title row
        RowLayout {
            id: titleRow

            width: forwardMessagePopup.innerWidth
            spacing: Komai.paddingSmall

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: titleLabel.font.pixelSize
                Layout.preferredWidth: titleLabel.font.pixelSize
                source: "image://colorimage/:/icons/icons/ui/forward.svg?" + palette.text
                sourceSize.height: titleLabel.font.pixelSize * Screen.devicePixelRatio
                sourceSize.width: titleLabel.font.pixelSize * Screen.devicePixelRatio
            }

            Label {
                id: titleLabel

                Layout.fillWidth: true
                color: palette.text
                font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.6)
                font.bold: true
                text: forwardMessagePopup.titleText()
            }

            Components.ImageButton {
                id: closeButton

                activeFocusOnTab: false
                toolTipText: qsTr("Close")
                toolTipVisible: hovered
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: titleLabel.font.pixelSize
                Layout.preferredWidth: titleLabel.font.pixelSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: forwardMessagePopup.close()
            }
        }

        // Intro
        Label {
            id: hintLabel

            width: forwardMessagePopup.innerWidth
            color: palette.buttonText
            font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.4)
            text: forwardMessagePopup.hintText()
            leftPadding: Komai.paddingSmall
            topPadding: Komai.paddingSmall
            bottomPadding: Komai.paddingSmall
            wrapMode: Text.Wrap
        }

        // Scrollable middle section: reply preview, room search and both list
        // panels. Its height is capped to whatever's left of the window after
        // the title, hint and action row, so on a short window this section
        // scrolls instead of pushing the Forward button off-screen.
        Flickable {
            id: forwardScrollArea

            width: forwardMessagePopup.innerWidth
            height: Math.min(forwardScrollColumn.implicitHeight, forwardMessagePopup.availableScrollHeight)
            contentWidth: width
            contentHeight: forwardScrollColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Components.FlickableWheelBooster {
                flickable: forwardScrollArea
            }

            ScrollBar.vertical: ScrollBar {
                id: forwardScrollBar

                policy: {
                    switch (forwardMessagePopup.scrollbarPolicySetting) {
                    case Settings.ScrollbarPolicy.Always:
                        return ScrollBar.AlwaysOn;
                    case Settings.ScrollbarPolicy.Never:
                        return ScrollBar.AlwaysOff;
                    default:
                        return forwardScrollArea.contentHeight > forwardScrollArea.height
                            ? ScrollBar.AlwaysOn
                            : ScrollBar.AlwaysOff;
                    }
                }
            }

            Column {
                id: forwardScrollColumn

                width: forwardMessagePopup.scrollContentWidth
                spacing: Komai.paddingSmall

                // Preview of the (single) message being forwarded.
                Loader {
                    id: replyPreviewLoader

                    active: forwardMessagePopup.showReplyPreview && forwardMessagePopup.messageCount === 1
                    width: forwardMessagePopup.scrollContentWidth
                    sourceComponent: replyPreviewComponent
                }

                Component {
                    id: replyPreviewComponent

                    Reply {
                        id: replyPreview

                        clickable: false
                        eventId: mid
                        room_: activeRoom
                        property bool isReplyFromCurrentUser: {
                            const currentUser = Komai.currentUser;
                            const currentUserId = (currentUser && currentUser.userid)
                                    ? String(currentUser.userid)
                                    : "";
                            return currentUserId.length > 0 && replyPreview.userId === currentUserId;
                        }
                        readonly property color previewWindowColor: (Komai.colors && Komai.colors.window !== undefined)
                            ? Komai.colors.window
                            : forwardMessagePopup.palette.window
                        readonly property color previewBaseColor: (Komai.colors && Komai.colors.base !== undefined)
                            ? Komai.colors.base
                            : forwardMessagePopup.palette.base
                        bubblePalette: activeRoom ? TimelineManager.roomUserBubblePalette(activeRoom.roomId, replyPreview.userId, roomColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userBubblePalette(replyPreview.userId, roomColor)
                        userColor: isReplyFromCurrentUser
                            ? Komai.theme.userColorSelf
                            : activeRoom ? TimelineManager.roomUserColor(activeRoom.roomId, replyPreview.userId, previewWindowColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyPreview.userId, previewWindowColor)
                        roomColor: isReplyFromCurrentUser
                            ? Komai.theme.userColorSelf
                            : activeRoom ? TimelineManager.roomUserColor(activeRoom.roomId, replyPreview.userId, previewBaseColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyPreview.userId, previewBaseColor)
                        limitHeight: true
                        width: forwardMessagePopup.scrollContentWidth
                        maxWidth: forwardMessagePopup.scrollContentWidth
                    }
                }

                // Room search
                Components.KomaiTextField {
                    id: roomTextInput

                    width: forwardMessagePopup.scrollContentWidth
                    font.pixelSize: Math.ceil(forwardMessagePopup.textHeight * 0.6)
                    implicitHeight: Math.max(controlHeight, Math.round(font.pixelSize * 2.0))
                    placeholderText: qsTr("Room name, address or id...")

                    Keys.onShortcutOverride: (event) => {
                        // Claim navigation / staging keys before the platform turns them
                        // into edit operations (Ctrl+U "clear line", etc.).
                        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                || ((event.key === Qt.Key_D || event.key === Qt.Key_U)
                                    && (event.modifiers & Qt.ControlModifier))
                                || (event.matches(StandardKey.InsertParagraphSeparator)
                                    && !(event.modifiers & Qt.ControlModifier)))
                            event.accepted = true;
                    }
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Up) {
                            suggestionList.moveSelection(-1);
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Down) {
                            suggestionList.moveSelection(1);
                            event.accepted = true;
                        } else if (event.key === Qt.Key_U && (event.modifiers & Qt.ControlModifier)) {
                            suggestionList.moveSelection(-forwardMessagePopup.pageStep);
                            event.accepted = true;
                        } else if (event.key === Qt.Key_D && (event.modifiers & Qt.ControlModifier)) {
                            suggestionList.moveSelection(forwardMessagePopup.pageStep);
                            event.accepted = true;
                        } else if (event.matches(StandardKey.InsertParagraphSeparator)
                                   && !(event.modifiers & Qt.ControlModifier)) {
                            forwardMessagePopup.stageCurrentSuggestion();
                            event.accepted = true;
                        }
                    }
                    onTextEdited: {
                        if (forwardMessagePopup.roomCompleter)
                            forwardMessagePopup.roomCompleter.searchString = text;
                    }
                }

                // --- Results (suggestions) ---
                Label {
                    width: forwardMessagePopup.scrollContentWidth
                    topPadding: Komai.paddingSmall
                    text: qsTr("Rooms")
                    color: palette.text
                    font.bold: true
                    font.pointSize: Settings.uiFontSizePt * 1.05
                }

                ListView {
                    id: suggestionList

                    // Reserve the scrollbar gutter so rows never sit under the bar.
                    readonly property real scrollGutter: forwardMessagePopup.scrollbarsReserveGutter
                        ? (suggestionScrollBar.implicitWidth + Komai.paddingSmall)
                        : 0

                    function moveSelection(delta) {
                        if (count === 0) {
                            currentIndex = -1;
                            return;
                        }
                        let next = (currentIndex < 0 ? 0 : currentIndex) + delta;
                        next = Math.max(0, Math.min(count - 1, next));
                        currentIndex = next;
                        positionViewAtIndex(next, ListView.Contain);
                    }

                    width: forwardMessagePopup.scrollContentWidth
                    height: forwardMessagePopup.reservedListHeight(7)
                    clip: true
                    model: forwardMessagePopup.roomCompleter
                    spacing: forwardMessagePopup.listSpacing
                    boundsBehavior: Flickable.StopAtBounds
                    highlightFollowsCurrentItem: true
                    keyNavigationEnabled: false

                    onCountChanged: {
                        if (count > 0 && (currentIndex < 0 || currentIndex >= count))
                            currentIndex = 0;
                        else if (count === 0)
                            currentIndex = -1;
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: suggestionScrollBar

                        policy: {
                            switch (forwardMessagePopup.scrollbarPolicySetting) {
                            case Settings.ScrollbarPolicy.Always:
                                return ScrollBar.AlwaysOn;
                            case Settings.ScrollbarPolicy.Never:
                                return ScrollBar.AlwaysOff;
                            default:
                                return suggestionList.contentHeight > suggestionList.height
                                    ? ScrollBar.AlwaysOn
                                    : ScrollBar.AlwaysOff;
                            }
                        }
                    }

                    delegate: AbstractButton {
                        id: suggestionDelegate

                        readonly property string rawRoomId: model.rawroomid
                        readonly property bool alreadySelected: forwardMessagePopup.recipientCount >= 0
                            && forwardMessagePopup.containsRecipient(model.rawroomid)
                        readonly property bool current: model.index === suggestionList.currentIndex

                        width: suggestionList.width - suggestionList.scrollGutter
                        implicitHeight: suggestionRow.implicitHeight + Komai.paddingSmall * 2
                        leftPadding: Komai.paddingMedium
                        rightPadding: Komai.paddingMedium
                        enabled: !alreadySelected
                        hoverEnabled: true
                        activeFocusOnTab: false
                        onClicked: {
                            suggestionList.currentIndex = model.index;
                            forwardMessagePopup.addRecipient(model.rawroomid);
                        }
                        onHoveredChanged: {
                            if (hovered && enabled)
                                suggestionList.currentIndex = model.index;
                        }

                        background: Rectangle {
                            radius: Komai.paddingMedium
                            color: {
                                if (suggestionDelegate.current && suggestionDelegate.enabled)
                                    return palette.highlight;
                                if (suggestionDelegate.alreadySelected)
                                    return Qt.tint(palette.window,
                                                   Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.22));
                                return palette.window;
                            }
                            border.width: 1
                            border.color: suggestionDelegate.alreadySelected
                                ? palette.highlight
                                : Komai.theme.separator
                        }

                        contentItem: RowLayout {
                            id: suggestionRow

                            spacing: Komai.paddingMedium

                            Components.Avatar {
                                Layout.preferredWidth: Komai.iconSize
                                Layout.preferredHeight: Komai.iconSize
                                Layout.alignment: Qt.AlignVCenter
                                displayName: model.roomName
                                roomid: model.rawroomid
                                url: (model.avatarUrl || "").replace("mxc://", "image://MxcImage/")
                                enabled: false
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: model.roomName
                                textFormat: Text.PlainText
                                color: suggestionDelegate.current && suggestionDelegate.enabled
                                    ? palette.highlightedText
                                    : suggestionDelegate.alreadySelected
                                        ? palette.buttonText
                                        : palette.text
                                font.italic: model.isTombstoned
                                font.pointSize: Settings.uiFontSizePt
                                elide: Text.ElideRight
                            }

                            Label {
                                visible: model.isSpace
                                Layout.alignment: Qt.AlignVCenter
                                text: qsTr("(Space)")
                                color: suggestionDelegate.current && suggestionDelegate.enabled
                                    ? palette.highlightedText
                                    : palette.buttonText
                                font.pointSize: Settings.uiFontSizePt * 0.9
                            }

                            Image {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: visible ? 18 : 0
                                Layout.preferredHeight: 18
                                visible: suggestionDelegate.alreadySelected
                                fillMode: Image.PreserveAspectFit
                                source: visible
                                    ? "image://colorimage/:/icons/icons/ui/double-checkmark.svg?" + palette.highlight
                                    : ""
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - Komai.paddingLarge * 2
                        visible: suggestionList.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 0.95
                        text: (roomTextInput.text.trim().length > 0)
                            ? qsTr("No matching rooms found.")
                            : qsTr("Start typing to find a room.")
                    }
                }

                // --- Staging (selected recipients) ---
                Label {
                    width: forwardMessagePopup.scrollContentWidth
                    topPadding: Komai.paddingSmall
                    text: forwardMessagePopup.selectedHeadingText()
                    color: palette.text
                    font.bold: true
                    font.pointSize: Settings.uiFontSizePt * 1.05
                }

                ListView {
                    id: recipientsList

                    readonly property real scrollGutter: forwardMessagePopup.scrollbarsReserveGutter
                        ? (recipientsScrollBar.implicitWidth + Komai.paddingSmall)
                        : 0

                    width: forwardMessagePopup.scrollContentWidth
                    height: forwardMessagePopup.reservedListHeight(5)
                    clip: true
                    model: recipientsModel
                    spacing: forwardMessagePopup.listSpacing
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar {
                        id: recipientsScrollBar

                        policy: {
                            switch (forwardMessagePopup.scrollbarPolicySetting) {
                            case Settings.ScrollbarPolicy.Always:
                                return ScrollBar.AlwaysOn;
                            case Settings.ScrollbarPolicy.Never:
                                return ScrollBar.AlwaysOff;
                            default:
                                return recipientsList.contentHeight > recipientsList.height
                                    ? ScrollBar.AlwaysOn
                                    : ScrollBar.AlwaysOff;
                            }
                        }
                    }

                    delegate: Rectangle {
                        width: recipientsList.width - recipientsList.scrollGutter
                        implicitHeight: recipientRow.implicitHeight + Komai.paddingSmall * 2
                        color: palette.window
                        radius: Komai.paddingMedium
                        border.color: Komai.theme.separator
                        border.width: 1

                        RowLayout {
                            id: recipientRow

                            anchors.fill: parent
                            anchors.leftMargin: Komai.paddingMedium
                            anchors.rightMargin: Komai.paddingSmall
                            anchors.topMargin: Komai.paddingSmall
                            anchors.bottomMargin: Komai.paddingSmall
                            spacing: Komai.paddingMedium

                            Components.Avatar {
                                Layout.preferredWidth: Komai.iconSize
                                Layout.preferredHeight: Komai.iconSize
                                Layout.alignment: Qt.AlignVCenter
                                displayName: model.roomName
                                roomid: model.roomId
                                url: (model.avatarUrl || "").replace("mxc://", "image://MxcImage/")
                                enabled: false
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: model.roomName
                                color: palette.text
                                font.pointSize: Settings.uiFontSizePt
                                elide: Text.ElideRight
                            }

                            Components.ImageButton {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: Komai.iconSize
                                Layout.preferredHeight: Komai.iconSize
                                activeFocusOnTab: false
                                hoverEnabled: true
                                toolTipText: qsTr("Remove")
                                toolTipVisible: hovered
                                image: ":/icons/icons/ui/dismiss.svg"
                                onClicked: forwardMessagePopup.removeRecipient(model.roomId)
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - Komai.paddingLarge * 2
                        visible: recipientsList.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 0.95
                        text: qsTr("Choose one or more rooms to forward the message to.")
                    }
                }
            } // forwardScrollColumn
        } // forwardScrollArea

        // Action row
        RowLayout {
            id: actionRow

            width: forwardMessagePopup.innerWidth
            spacing: Komai.paddingMedium

            Item {
                Layout.fillWidth: true
            }

            Components.KomaiButton {
                id: forwardButton

                activeFocusOnTab: true
                focusPolicy: Qt.StrongFocus
                highlighted: true
                enabled: forwardMessagePopup.recipientCount > 0
                text: qsTr("Forward")
                onClicked: forwardMessagePopup.forwardToRecipients()
                Keys.onEnterPressed: event => {
                    forwardMessagePopup.forwardToRecipients();
                    event.accepted = true;
                }
                Keys.onReturnPressed: event => {
                    forwardMessagePopup.forwardToRecipients();
                    event.accepted = true;
                }
            }
        }
    }
}
