// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Page {
    id: roomListPage
    //leftPadding: Komai.paddingSmall
    //rightPadding: Komai.paddingSmall
    required property var adaptiveView
    required property var timelineRoot
    required property var tabController
    property int density: Komai.density
    property int avatarSize: Komai.iconSize
    property var communitiesTarget: null

    // Row layout metrics mirrored from RoomListItemDelegate (and the matching
    // space-header / footer rows) so we can compute the widths at which the
    // sidebar renders without clipping the avatar.  All inner rows share the
    // same outer paddingMedium+paddingSmall margin on each side and an
    // avatarSize-wide leading icon, so that's the only floor we need.
    readonly property int iconOnlyMinWidth: 2 * (Komai.paddingMedium + Komai.paddingSmall)
        + avatarSize
    // The room list has no depth indent or other "full layout" metric beyond
    // the icon-only minimum, so the dead zone above iconOnlyMinWidth is just
    // a small slack region in which the splitter still snaps to icon-only on
    // release rather than wedging a barely-useful sliver of text content.
    readonly property int fullMinWidth: iconOnlyMinWidth + Komai.paddingMedium

    // True when the sidebar is too narrow to render the full layout; in this
    // mode rows, the space header, the to-top button, and footers all
    // collapse to icon-only so the avatar is never clipped.
    readonly property bool iconOnly: width < fullMinWidth
    property bool interactionSuppressed: false
    readonly property Item roomListLastActionButton: null
    property bool pendingGoToTopRequest: false

    function eventMatchesLatinKey(event, latinKey) {
        if (!event)
            return false;

        return LayoutAgnosticKeys.matchesLatinKey(latinKey,
                                                  event.key,
                                                  event.nativeScanCode);
    }

    function eventUsesNoModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) === 0;
    }

    function eventUsesNavigationModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function eventUsesCtrlOnlyModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & Qt.ControlModifier) !== 0
            && (modifiers & (Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) === 0;
    }

    function eventUsesShiftOnlyModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & Qt.ShiftModifier) !== 0
            && (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function resetGoToTopSequence() {
        pendingGoToTopRequest = false;
        goToTopSequenceTimer.stop();
    }

    function isBackwardTabEvent(event) {
        if (!event)
            return false;

        const modifiers = Number(event.modifiers);
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) !== 0)
            return false;

        return event.key === Qt.Key_Backtab
            || (event.key === Qt.Key_Tab && (modifiers & Qt.ShiftModifier) !== 0);
    }

    function isForwardTabEvent(event) {
        if (!event)
            return false;

        const modifiers = Number(event.modifiers);
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) !== 0)
            return false;

        return event.key === Qt.Key_Tab;
    }

    function focusCommunities() {
        if (!communitiesTarget || !communitiesTarget.visible)
            return false;

        return communitiesTarget.focusKeyboardNavigation();
    }

    function updateInteractionSuppression() {
        interactionSuppressed = visible
            && (roomListInteractionHoverHandler.hovered || roomlist.activeFocus || roomListFreezeIndicator.indicatorHovered || roomListToTopButton.hovered || roomListContextMenu.visible);
        Rooms.setInteractionSuppressed(interactionSuppressed);
    }

    function focusKeyboardNavigation() {
        if (!visible)
            return false;

        Qt.callLater(function () {
            roomlist.seedKeyboardCursor();
            roomlist.forceActiveFocus(Qt.ShortcutFocusReason);
        });
        return true;
    }

    onVisibleChanged: updateInteractionSuppression()

    Component.onCompleted: updateInteractionSuppression()

    Component.onDestruction: Rooms.setInteractionSuppressed(false)

    ComponentCatalog {
        id: componentCatalog
    }

    background: Rectangle {
        color: Komai.theme.sidebarBackground
    }
    Shortcut {
        sequences: ["Ctrl+Shift+R"]
        context: Qt.ApplicationShortcut
        enabled: roomListPage.visible

        onActivated: roomListPage.focusKeyboardNavigation()
        onActivatedAmbiguously: roomListPage.focusKeyboardNavigation()
    }
    Timer {
        id: goToTopSequenceTimer

        interval: 500
        repeat: false
        onTriggered: roomListPage.pendingGoToTopRequest = false
    }
    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            roomListContextMenu.close();
        }

        target: MainWindow
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RoomListSpaceHeader {
            Layout.fillWidth: true
            collapsed: roomListPage.iconOnly
            avatarSize: roomListPage.avatarSize
        }

        ListView {
            id: roomlist

            // True until we have scrolled the active room into view once
            // (typically the saved current room on startup). After that we
            // never touch the viewport in response to current-room changes,
            // so tab switches don't yank the user's scroll position.
            property bool _initialScrollPending: true

            readonly property bool hasVerticalOverflow: contentHeight > height
            readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
            readonly property bool scrollbarVisible: {
                if (roomListPage.iconOnly)
                    return false;
                switch (scrollbarPolicy) {
                case Settings.ScrollbarPolicy.Always:
                    return true;
                case Settings.ScrollbarPolicy.Never:
                    return false;
                case Settings.ScrollbarPolicy.WhenNeeded:
                default:
                    return hasVerticalOverflow;
                }
            }
            readonly property real reservedScrollbarWidth: scrollbarVisible
                ? Math.max(scrollbar.width, scrollbar.implicitWidth)
                : 0

            function ensureKeyboardCursorVisible() {
                if (currentIndex < 0 || currentIndex >= count)
                    return;

                positionViewAtIndex(currentIndex, ListView.Contain);
            }

            function seedKeyboardCursor() {
                if (count <= 0) {
                    currentIndex = -1;
                    return;
                }

                const currentRoomId = Rooms.currentRoomId;
                const selectedIndex = currentRoomId ? Rooms.roomidToIndex(currentRoomId) : -1;

                currentIndex = selectedIndex >= 0 ? selectedIndex : 0;
                ensureKeyboardCursorVisible();
            }

            function moveKeyboardCursor(delta) {
                if (count <= 0)
                    return;

                if (currentIndex < 0) {
                    seedKeyboardCursor();
                    return;
                }

                currentIndex = Math.max(0, Math.min(count - 1, currentIndex + delta));
                ensureKeyboardCursorVisible();
            }

            function moveKeyboardCursorByChunk(delta) {
                if (count <= 0)
                    return;

                const halfScreenRows = Math.max(1, Math.floor((height / Math.max(1, Komai.navigationRowHeight)) / 2));
                moveKeyboardCursor(delta * halfScreenRows);
            }

            function activateKeyboardCursor() {
                if (currentIndex < 0 || currentIndex >= count || !Rooms.roomIdAt)
                    return;

                const roomId = Rooms.roomIdAt(currentIndex);
                if (!roomId)
                    return;

                Rooms.setCurrentRoom(roomId);
                ensureKeyboardCursorVisible();
            }

            function goToFirstItem() {
                if (count <= 0)
                    return;

                currentIndex = 0;
                ensureKeyboardCursorVisible();
            }

            function goToLastItem() {
                if (count <= 0)
                    return;

                currentIndex = count - 1;
                ensureKeyboardCursorVisible();
            }

            HoverHandler {
                id: roomListInteractionHoverHandler

                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
                onHoveredChanged: roomListPage.updateInteractionSuppression()
            }

            Layout.fillWidth: true
            Layout.fillHeight: true
            activeFocusOnTab: false
            clip: true
            model: Rooms
            boundsBehavior: Flickable.StopAtBounds

            Keys.onShortcutOverride: event => {
                if (roomListPage.isForwardTabEvent(event) || roomListPage.isBackwardTabEvent(event))
                    event.accepted = true;
            }
            Keys.priority: Keys.BeforeItem
            Keys.onPressed: event => {
                const gKeyPressed = roomListPage.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.G);
                const plainGPressed = gKeyPressed && roomListPage.eventUsesNoModifiers(event);
                const shiftGPressed = gKeyPressed && roomListPage.eventUsesShiftOnlyModifiers(event);

                if (roomListPage.isForwardTabEvent(event) || roomListPage.isBackwardTabEvent(event)) {
                    roomListPage.resetGoToTopSequence();
                    event.accepted = roomListPage.focusCommunities();
                    return;
                }

                if (!plainGPressed)
                    roomListPage.resetGoToTopSequence();

                if ((event.key === Qt.Key_Up
                            || roomListPage.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.K))
                        && roomListPage.eventUsesNavigationModifiers(event)) {
                    roomlist.moveKeyboardCursor(-1);
                    event.accepted = true;
                    return;
                }

                if ((event.key === Qt.Key_Down
                            || roomListPage.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.J))
                        && roomListPage.eventUsesNavigationModifiers(event)) {
                    roomlist.moveKeyboardCursor(1);
                    event.accepted = true;
                    return;
                }

                if (roomListPage.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.U)
                        && roomListPage.eventUsesCtrlOnlyModifiers(event)) {
                    roomlist.moveKeyboardCursorByChunk(-1);
                    event.accepted = true;
                    return;
                }

                if (roomListPage.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.D)
                        && roomListPage.eventUsesCtrlOnlyModifiers(event)) {
                    roomlist.moveKeyboardCursorByChunk(1);
                    event.accepted = true;
                    return;
                }

                if (shiftGPressed) {
                    roomlist.goToLastItem();
                    event.accepted = true;
                    return;
                }

                if (plainGPressed) {
                    if (roomListPage.pendingGoToTopRequest) {
                        roomListPage.resetGoToTopSequence();
                        roomlist.goToFirstItem();
                    } else {
                        roomListPage.pendingGoToTopRequest = true;
                        goToTopSequenceTimer.restart();
                    }
                    event.accepted = true;
                    return;
                }

                switch (event.key) {
                case Qt.Key_Home:
                    roomlist.goToFirstItem();
                    event.accepted = true;
                    break;
                case Qt.Key_End:
                    roomlist.goToLastItem();
                    event.accepted = true;
                    break;
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    roomlist.activateKeyboardCursor();
                    event.accepted = true;
                    break;
                case Qt.Key_Escape:
                    TimelineManager.focusMessageInput();
                    event.accepted = true;
                    break;
                default:
                    break;
                }
            }

            //reuseItems: true
            ScrollBar.vertical: ScrollBar {
                id: scrollbar

                parent: roomlist
                activeFocusOnTab: false
                focusPolicy: Qt.NoFocus
                policy: roomlist.scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                palette.dark: Qt.darker(parent.palette.alternateBase, 1.5)
                palette.mid: Qt.darker(parent.palette.alternateBase, 1.3)

                Rectangle {
                    anchors.fill: parent
                    color: palette.window
                    z: -1
                }
            }
            delegate: RoomListItemDelegate {
                density: roomListPage.density
                avatarSize: roomListPage.avatarSize
                collapsed: roomListPage.iconOnly
                roomContextMenu: roomListContextMenu
                scrollbarReservedWidth: roomlist.reservedScrollbarWidth
                tabController: roomListPage.tabController
            }

            Connections {
                function onCurrentRoomIdChanged() {
                    const roomId = Rooms.currentRoomId;
                    if (!roomId)
                        return;
                    if (!roomlist._initialScrollPending)
                        return;

                    Qt.callLater(function () {
                        if (!roomlist._initialScrollPending)
                            return;

                        const activeRoomId = Rooms.currentRoomId;
                        if (!activeRoomId || activeRoomId !== roomId)
                            return;

                        const index = Rooms.roomidToIndex(roomId);
                        if (index < 0)
                            return;

                        roomlist._initialScrollPending = false;

                        if (roomlist.activeFocus)
                            roomlist.currentIndex = index;
                        if (TimelineManager.roomSwitchPerfEnabled())
                            TimelineManager.markRoomSwitchPhase(roomId, "qml.room_list.scroll_into_view.begin");
                        roomlist.positionViewAtIndex(index, ListView.Contain);
                        if (TimelineManager.roomSwitchPerfEnabled())
                            TimelineManager.markRoomSwitchPhase(roomId, "qml.room_list.scroll_into_view.end");
                    });
                }

                target: Rooms
            }
            Component {
                id: roomWindowComponent

                DetachedRoomWindow {
                }
            }
            RoomListContextMenu {
                id: roomListContextMenu

                timelineRoot: roomListPage.timelineRoot
                roomWindowComponent: roomWindowComponent
                tabController: roomListPage.tabController
                onVisibleChanged: roomListPage.updateInteractionSuppression()
            }
            RoomListToTopButton {
                id: roomListToTopButton
                roomList: roomlist
                scrollbarItem: scrollbar
                collapsed: roomListPage.iconOnly
                onHoveredChanged: roomListPage.updateInteractionSuppression()
            }
            RoomListFreezeIndicator {
                id: roomListFreezeIndicator
                roomList: roomlist
                suppressed: roomListPage.interactionSuppressed && Rooms.hasSuppressedUpdates
            }

            footer: Column {
                id: roomListFooter

                width: roomlist.width

                RoomListExploreFooter {
                    id: exploreFooter
                    width: parent.width
                    collapsed: roomListPage.iconOnly
                    tabController: roomListPage.tabController
                }
                RoomListBotChatFooter {
                    id: botChatFooter
                    width: parent.width
                    collapsed: roomListPage.iconOnly
                }
            }

            onActiveFocusChanged: {
                roomListPage.updateInteractionSuppression();
                if (activeFocus)
                    seedKeyboardCursor();
            }
        }
    }

    // Right-click on empty room list space shows settings shortcut.
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: roomListSettingsMenu.popup()
    }

    Menu {
        id: roomListSettingsMenu

        Component.onCompleted: {
            if (roomListSettingsMenu.popupType != undefined)
                roomListSettingsMenu.popupType = 2;
        }

        MenuItem {
            text: qsTr("Settings...") // Keep short: Qt may clip/elide longer menu item text
            icon.source: "qrc:/icons/icons/ui/settings.svg"

            onTriggered: MainWindow.showUserSettingsPage(
                UserSettingsModel.TabNavigation,
                "navigation-room-list-section")
        }
    }
}
