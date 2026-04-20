// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Page {
    id: communitySidebar

    //leftPadding: Komai.paddingSmall
    //rightPadding: Komai.paddingSmall
    required property var adaptiveView
    property int avatarSize: Komai.iconSize
    property var roomListTarget: null
    property bool pendingGoToTopRequest: false

    // Row layout metrics mirrored from CommunitiesListItemDelegate so we can
    // compute the widths at which rows render without clipping. Two row
    // shapes need to fit at the icon-only minimum:
    //  - Non-collapsible rows use leading + trailing fillWidth spacers,
    //    adding a paddingMedium of spacing on both sides of the avatar.
    //  - Collapsible rows (only possible when a space has children) use a
    //    chevron + a single spacing before the avatar; no trailing spacing.
    readonly property int rowLineSpacing: Math.max(1, Math.round(Komai.fontPixelSize * 1.2))
    readonly property real deepestAvatarSize: Math.max(avatarSize * 0.5,
        Math.round(avatarSize * Math.pow(0.85, Communities.maxDepth)))
    readonly property int outerRowPadding: 2 * (Komai.paddingMedium + Komai.paddingSmall)
    readonly property int nonCollapsibleRowMinWidth: outerRowPadding
        + 2 * Komai.paddingMedium
        + avatarSize
    readonly property int collapsibleRowMinWidth: outerRowPadding
        + rowLineSpacing + Komai.paddingMedium
        + avatarSize
    readonly property int iconOnlyMinWidth: Communities.containsSubspaces
        ? Math.max(nonCollapsibleRowMinWidth, collapsibleRowMinWidth)
        : nonCollapsibleRowMinWidth
    readonly property int fullMinWidth: iconOnlyMinWidth
        + rowLineSpacing * Communities.maxDepth
        + Komai.paddingMedium
        - avatarSize
        + deepestAvatarSize

    // True when the sidebar is too narrow to render the full layout; in this
    // mode we collapse rows and the profile header to icon-only so the
    // deepest subspace avatar is never clipped by the depth indent.
    readonly property bool iconOnly: width < fullMinWidth

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

    function focusRoomList() {
        if (!roomListTarget || !roomListTarget.visible)
            return false;

        return roomListTarget.focusKeyboardNavigation();
    }

    function focusKeyboardNavigation() {
        if (!visible)
            return false;

        Qt.callLater(function () {
            communitiesList.seedKeyboardCursor();
            communitiesList.forceActiveFocus(Qt.ShortcutFocusReason);
        });
        return true;
    }

    background: Rectangle {
        color: Komai.theme.sidebarBackground
    }

    Shortcut {
        sequences: ["Ctrl+Shift+C"]
        context: Qt.ApplicationShortcut
        enabled: communitySidebar.visible

        onActivated: communitySidebar.focusKeyboardNavigation()
        onActivatedAmbiguously: communitySidebar.focusKeyboardNavigation()
    }
    Timer {
        id: goToTopSequenceTimer

        interval: 500
        repeat: false
        onTriggered: communitySidebar.pendingGoToTopRequest = false
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            communitySidebarContextMenu.close();
        }

        target: MainWindow
    }
    ListView {
        id: communitiesList

        readonly property bool hasVerticalOverflow: contentHeight > height
        readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
        readonly property bool scrollbarVisible: {
            if (communitySidebar.iconOnly)
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

            const selectedIndex = model && model.filterIdToIndex
                ? model.filterIdToIndex(Communities.currentFilterId)
                : -1;

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
            if (currentIndex < 0 || currentIndex >= count || !model || !model.filterIdAt)
                return;

            Communities.setCurrentFilterId(model.filterIdAt(currentIndex));
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

        function currentKeyboardItem() {
            if (currentIndex < 0 || currentIndex >= count)
                return null;

            ensureKeyboardCursorVisible();
            return currentItem;
        }

        function expandCurrentSpace() {
            const item = currentKeyboardItem();
            if (!item || !item.model || !item.model.collapsible || !item.model.collapsed)
                return false;

            item.model.collapsed = false;
            ensureKeyboardCursorVisible();
            return true;
        }

        function collapseCurrentSpace() {
            const item = currentKeyboardItem();
            if (!item || !item.model || !item.model.collapsible || item.model.collapsed)
                return false;

            item.model.collapsed = true;
            ensureKeyboardCursorVisible();
            return true;
        }

        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height
        activeFocusOnTab: false
        clip: true
        model: Communities.filtered()
        boundsBehavior: Flickable.StopAtBounds

        header: Rectangle {
            id: userSettingsItem

            readonly property var profile: Komai.currentUser
            readonly property string mxid: profile ? profile.userid : ""
            readonly property string displayName: profile ? profile.displayName : ""
            readonly property bool showSecondRow: displayName.length > 0
                && Komai.density !== Settings.Density.Dense
            readonly property bool isHovered: hoverHandler.hovered
            readonly property color textColor: isHovered ? palette.brightText : palette.text
            readonly property color secondaryTextColor: isHovered ? palette.brightText : palette.buttonText

            width: communitiesList.width - communitiesList.reservedScrollbarWidth
            height: Komai.navigationRowHeight
            color: isHovered ? palette.dark : palette.alternateBase

            HoverHandler {
                id: hoverHandler
            }

            KomaiCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                acceptedButtons: Qt.LeftButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onSingleTapped: MainWindow.showUserSettingsPage()
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onSingleTapped: userSettingsContextMenu.popup()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Komai.paddingMedium + Komai.paddingSmall
                anchors.rightMargin: Komai.paddingMedium + Komai.paddingSmall
                anchors.topMargin: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingMedium
                anchors.bottomMargin: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingMedium
                spacing: Komai.paddingMedium

                UserSettingsFlipButton {
                    Layout.alignment: communitySidebar.iconOnly ? Qt.AlignCenter : Qt.AlignVCenter
                    Layout.fillWidth: communitySidebar.iconOnly
                    Layout.preferredHeight: communitySidebar.avatarSize
                    Layout.preferredWidth: communitySidebar.avatarSize
                    avatarButtonSize: communitySidebar.avatarSize
                    flipAngle: (activeFocus || userSettingsItem.isHovered) ? 180 : 0
                    externalActive: userSettingsItem.isHovered

                    onLeftClicked: MainWindow.showUserSettingsPage()
                    onRightClicked: userSettingsContextMenu.popup()
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                    spacing: 0
                    visible: !communitySidebar.iconOnly

                    ElidedLabel {
                        Layout.fillWidth: true
                        color: userSettingsItem.textColor
                        elideWidth: width
                        fullText: userSettingsItem.mxid
                        textFormat: Text.PlainText
                        font.pixelSize: userSettingsItem.showSecondRow ? Komai.fontPixelSize * 0.9 : Komai.fontPixelSize
                    }

                    ElidedLabel {
                        Layout.fillWidth: true
                        color: userSettingsItem.secondaryTextColor
                        elideWidth: width
                        fullText: userSettingsItem.displayName
                        textFormat: Text.PlainText
                        font.pixelSize: Komai.fontPixelSize * 0.9
                        visible: userSettingsItem.showSecondRow
                    }
                }
            }

            RoomListProfileMenu {
                id: userSettingsContextMenu
            }
        }

        Keys.onShortcutOverride: event => {
            if (communitySidebar.isForwardTabEvent(event) || communitySidebar.isBackwardTabEvent(event))
                event.accepted = true;
        }
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: event => {
            const gKeyPressed = communitySidebar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.G);
            const plainGPressed = gKeyPressed && communitySidebar.eventUsesNoModifiers(event);
            const shiftGPressed = gKeyPressed && communitySidebar.eventUsesShiftOnlyModifiers(event);

            if (communitySidebar.isForwardTabEvent(event) || communitySidebar.isBackwardTabEvent(event)) {
                communitySidebar.resetGoToTopSequence();
                event.accepted = communitySidebar.focusRoomList();
                return;
            }

            if (!plainGPressed)
                communitySidebar.resetGoToTopSequence();

            if ((event.key === Qt.Key_Up
                        || communitySidebar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.K))
                    && communitySidebar.eventUsesNavigationModifiers(event)) {
                communitiesList.moveKeyboardCursor(-1);
                event.accepted = true;
                return;
            }

            if ((event.key === Qt.Key_Down
                        || communitySidebar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.J))
                    && communitySidebar.eventUsesNavigationModifiers(event)) {
                communitiesList.moveKeyboardCursor(1);
                event.accepted = true;
                return;
            }

            if ((event.key === Qt.Key_Left
                        || communitySidebar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.H))
                    && communitySidebar.eventUsesNavigationModifiers(event)) {
                communitiesList.collapseCurrentSpace();
                event.accepted = true;
                return;
            }

            if ((event.key === Qt.Key_Right
                        || communitySidebar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.L))
                    && communitySidebar.eventUsesNavigationModifiers(event)) {
                communitiesList.expandCurrentSpace();
                event.accepted = true;
                return;
            }

            if (communitySidebar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.U)
                    && communitySidebar.eventUsesCtrlOnlyModifiers(event)) {
                communitiesList.moveKeyboardCursorByChunk(-1);
                event.accepted = true;
                return;
            }

            if (communitySidebar.eventMatchesLatinKey(event, LayoutAgnosticKeys.LatinKey.D)
                    && communitySidebar.eventUsesCtrlOnlyModifiers(event)) {
                communitiesList.moveKeyboardCursorByChunk(1);
                event.accepted = true;
                return;
            }

            if (shiftGPressed) {
                communitiesList.goToLastItem();
                event.accepted = true;
                return;
            }

            if (plainGPressed) {
                if (communitySidebar.pendingGoToTopRequest) {
                    communitySidebar.resetGoToTopSequence();
                    communitiesList.goToFirstItem();
                } else {
                    communitySidebar.pendingGoToTopRequest = true;
                    goToTopSequenceTimer.restart();
                }
                event.accepted = true;
                return;
            }

            switch (event.key) {
            case Qt.Key_Home:
                communitiesList.goToFirstItem();
                event.accepted = true;
                break;
            case Qt.Key_End:
                communitiesList.goToLastItem();
                event.accepted = true;
                break;
            case Qt.Key_Return:
            case Qt.Key_Enter:
                communitiesList.activateKeyboardCursor();
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

        ScrollBar.vertical: ScrollBar {
            id: scrollbar

            parent: communitiesList
            policy: communitiesList.scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            palette.dark: Qt.darker(parent.palette.alternateBase, 1.5)
            palette.mid: Qt.darker(parent.palette.alternateBase, 1.3)

            Rectangle {
                anchors.fill: parent
                color: palette.window
                z: -1
            }
        }
        delegate: CommunitiesListItemDelegate {
            avatarSize: communitySidebar.avatarSize
            collapsed: communitySidebar.iconOnly
            communityContextMenu: communitySidebarContextMenu
            scrollbarReservedWidth: communitiesList.reservedScrollbarWidth
        }

        CommunitiesContextMenu {
            id: communitySidebarContextMenu

            onHideFilterRequested: {
                hideFilterDialog.tagId = communitySidebarContextMenu.tagId;
                hideFilterDialog.filterName = communitySidebarContextMenu.displayName;
                hideFilterDialog.open();
            }
        }

        onActiveFocusChanged: {
            if (activeFocus)
                seedKeyboardCursor();
        }
    }

    HideFilterDialog {
        id: hideFilterDialog
    }

    // Right-click on empty sidebar space shows settings shortcut.
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: communitiesSidebarSettingsMenu.popup()
    }

    Menu {
        id: communitiesSidebarSettingsMenu

        Component.onCompleted: {
            if (communitiesSidebarSettingsMenu.popupType != undefined)
                communitiesSidebarSettingsMenu.popupType = 2;
        }

        MenuItem {
            text: qsTr("Settings...") // Keep short: Qt may clip/elide longer menu item text
            icon.source: "qrc:/icons/icons/ui/settings.svg"

            onTriggered: MainWindow.showUserSettingsPage(
                UserSettingsModel.TabNavigation,
                "navigation-communities-sidebar-section")
        }
    }
}
