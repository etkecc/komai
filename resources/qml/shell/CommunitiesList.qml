// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQml
import QtQuick
import QtQuick.Controls
import cc.etke.komai

Page {
    id: communitySidebar

    //leftPadding: Komai.paddingSmall
    //rightPadding: Komai.paddingSmall
    required property var adaptiveView
    property int avatarSize: Komai.listIconSize
    property bool collapsed: false

    function focusKeyboardNavigation() {
        if (!visible)
            return false;

        if (adaptiveView && adaptiveView.singlePageMode)
            adaptiveView.pageIndex = 0;

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
            if (communitySidebar.collapsed)
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

        function activateKeyboardCursor() {
            if (currentIndex < 0 || currentIndex >= count || !model || !model.filterIdAt)
                return;

            Communities.setCurrentFilterId(model.filterIdAt(currentIndex));
            ensureKeyboardCursorVisible();
        }

        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height
        activeFocusOnTab: false
        clip: true
        model: Communities.filtered()
        boundsBehavior: Flickable.StopAtBounds

        Keys.priority: Keys.BeforeItem
        Keys.onPressed: event => {
            switch (event.key) {
            case Qt.Key_Up:
                communitiesList.moveKeyboardCursor(-1);
                event.accepted = true;
                break;
            case Qt.Key_Down:
                communitiesList.moveKeyboardCursor(1);
                event.accepted = true;
                break;
            case Qt.Key_Home:
                if (communitiesList.count > 0) {
                    communitiesList.currentIndex = 0;
                    communitiesList.ensureKeyboardCursorVisible();
                }
                event.accepted = true;
                break;
            case Qt.Key_End:
                if (communitiesList.count > 0) {
                    communitiesList.currentIndex = communitiesList.count - 1;
                    communitiesList.ensureKeyboardCursorVisible();
                }
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
            collapsed: communitySidebar.collapsed
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
}
