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
    property bool compactMode: Komai.uiLayoutCompactMode
    property int avatarSize: Komai.listIconSize
    property bool collapsed: false
    property var communitiesTarget: null
    readonly property Item roomListLastActionButton: roomActionsBar.lastFocusableActionButton
    property bool pendingGoToTopRequest: false
    readonly property var profileMenu: profileContextMenu

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

    function focusKeyboardNavigation() {
        if (!visible)
            return false;

        if (adaptiveView && adaptiveView.singlePageMode)
            adaptiveView.pageIndex = 1;

        Qt.callLater(function () {
            roomlist.seedKeyboardCursor();
            roomlist.forceActiveFocus(Qt.ShortcutFocusReason);
        });
        return true;
    }

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
    header: ColumnLayout {
        spacing: 0

        RoomListUserInfoPanel {
            id: userInfoPanel
            collapsed: roomListPage.collapsed
            Layout.fillWidth: true
        }
        Rectangle {
            Layout.fillWidth: true
            color: Komai.theme.separator
            Layout.preferredHeight: Settings.sidebarsCommunitiesVisible ? 0 : 2
        }
        RoomListActionsBar {
            id: roomActionsBar

            Layout.fillWidth: true
            Layout.preferredHeight: Komai.navigationRowHeight
            avatarSize: roomListPage.avatarSize
            profileContextMenu: profileContextMenu
            componentCatalog: componentCatalog
            timelineRoot: roomListPage.timelineRoot
        }
        Rectangle {
            Layout.fillWidth: true
            color: Komai.theme.separator
            Layout.preferredHeight: 1
        }
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            userInfoPanel.closeMenu();
            roomListContextMenu.close();
        }

        target: MainWindow
    }
    RoomListProfileMenu {
        id: profileContextMenu

        timelineRoot: roomListPage.timelineRoot
        componentCatalog: componentCatalog
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RoomListSpaceHeader {
            Layout.fillWidth: true
            collapsed: roomListPage.collapsed
            avatarSize: roomListPage.avatarSize
        }

        ListView {
            id: roomlist

            readonly property bool hasVerticalOverflow: contentHeight > height
            readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
            readonly property bool scrollbarVisible: {
                if (roomListPage.collapsed)
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
                compactMode: roomListPage.compactMode
                avatarSize: roomListPage.avatarSize
                collapsed: roomListPage.collapsed
                roomContextMenu: roomListContextMenu
                scrollbarReservedWidth: roomlist.reservedScrollbarWidth
            }

            Connections {
                function onCurrentRoomIdChanged() {
                    const roomId = Rooms.currentRoomId;
                    if (!roomId)
                        return;

                    Qt.callLater(function () {
                        const activeRoomId = Rooms.currentRoomId;
                        if (!activeRoomId || activeRoomId !== roomId)
                            return;

                        const index = Rooms.roomidToIndex(roomId);
                        if (index < 0)
                            return;

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
            }
            RoomListToTopButton {
                roomList: roomlist
                scrollbarItem: scrollbar
                collapsed: roomListPage.collapsed
            }

            footer: Column {
                width: roomlist.width

                RoomListExploreFooter {
                    width: parent.width
                    collapsed: roomListPage.collapsed
                    timelineRoot: roomListPage.timelineRoot
                }
                RoomListBotChatFooter {
                    width: parent.width
                    collapsed: roomListPage.collapsed
                    profileContextMenu: roomListPage.profileMenu
                }
            }

            onActiveFocusChanged: {
                if (activeFocus)
                    seedKeyboardCursor();
            }
        }
    }
}
