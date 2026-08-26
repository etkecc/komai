// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Pane {
    id: timelineRoot

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    property var activeMediaOverlay: null
    property color overlayBackdropColor: Qt.rgba(0, 0, 0, palette.window.hslLightness < 0.5 ? 0.76 : 0.68)
    readonly property var rootTimeline: timelineRoot

    Login {
        id: sharedLoginController
    }

    Registration {
        id: sharedRegistrationController
    }

    ComponentCatalog {
        id: componentCatalog
    }

    function adjustFontSize(step) {
        const minFontSizePt = 6;
        const maxFontSizePt = 64;
        const next = Math.max(minFontSizePt, Math.min(maxFontSizePt, Settings.uiFontSizePt + step));
        if (next !== Settings.uiFontSizePt)
            Settings.uiFontSizePt = next;
    }

    function destroyOnClose(obj) {
        if (obj.closing != undefined)
            obj.closing.connect(() => obj.destroy(1000));
        else if (obj.aboutToHide != undefined)
            obj.aboutToHide.connect(() => obj.destroy(1000));
    }
    function destroyOnClosed(obj) {
        obj.aboutToHide.connect(() => obj.destroy(1000));
    }
    function createDialog(componentUrl, properties) {
        var component = Qt.createComponent(componentUrl);
        if (component.status !== Component.Ready) {
            console.error("Failed to create component: " + component.errorString());
            return null;
        }

        var dialog = component.createObject(timelineRoot, properties || {});
        if (!dialog)
            console.error("Failed to create dialog object for: " + componentUrl);
        return dialog;
    }
function openCatalogDialog(componentUrl, properties) {
        var dialog = createDialog(componentUrl, properties);
        if (!dialog)
            return null;
        dialog.open();
        destroyOnClose(dialog);
        return dialog;
    }
    function showForwardMessageDialog(room, eventIdsOrEventId, timeline, timelineView, selectionCount) {
        if (!room || !eventIdsOrEventId)
            return;

        const eventIds = Array.isArray(eventIdsOrEventId)
            ? eventIdsOrEventId
            : [eventIdsOrEventId];
        if (eventIds.length === 0)
            return;

        var dialog = createDialog(componentCatalog.navigationForwardCompleterDialog, {
                "roomSource": room,
                // Qt.binding() keeps this tracking the live viewport width; a
                // plain value here would freeze at whatever it was when the
                // dialog opened and never follow a later window resize.
                "dialogViewportWidth": Qt.binding(() => timelineRoot.width),
                "modalOverlayColor": timelineRoot.overlayBackdropColor,
                "timelineSource": timeline ?? null,
                "timelineViewSource": timelineView ?? null,
                "showReplyPreview": true
            });
        if (!dialog) {
            console.error("Failed to create ForwardCompleter object");
            return;
        }

        dialog.setMessageEventIds(eventIds, selectionCount);
        dialog.open();
        destroyOnClose(dialog);
        return dialog;
    }

    //Timer {
    //    onTriggered: gc()
    //    interval: 1000
    //    running: true
    //    repeat: true
    //}
    function showAliasEditor(settings) {
        openCatalogDialog(componentCatalog.roomAliasEditorDialog, {
                "roomSettings": settings
            });
    }
    function showAllowedRoomsEditor(settings) {
        openCatalogDialog(componentCatalog.roomAllowedRoomsSettingsDialog, {
                "roomSettings": settings
            });
    }
    function showPLEditor(settings) {
        openCatalogDialog(componentCatalog.roomPowerLevelEditorDialog, {
                "roomSettings": settings,
                "appRoot": timelineRoot
            });
    }
    function showSpacePLApplyPrompt(settings, editingModel) {
        openCatalogDialog(componentCatalog.roomPowerLevelSpacesApplyDialog, {
                "roomSettings": settings,
                "editingModel": editingModel
            });
    }

    background: null
    padding: 0

    FontMetrics {
        id: fontMetrics

        // Bind explicitly to Settings.uiFontSizePt and Komai.fontFamily rather
        // than inheriting from Qt.application.font. Qt Quick does not reliably
        // re-resolve the default-font cascade when QGuiApplication::setFont()
        // is called at runtime, so the derived ascent/lineSpacing/height
        // values stay frozen for the many callers that use this singleton
        // (TimelineMetadata iconSize, TypingIndicator row height, avatar
        // sizing via lineSpacing, etc.). Binding the source font to the
        // reactive settings directly gives those callers live updates.
        font.pointSize: Settings.uiFontSizePt
        font.family: Komai.fontFamily
    }
    UserDirectoryModel {
        id: userDirectory

    }
    RoomDirectoryModel {
        id: publicRooms

    }

    Loader {
        id: roomDirectoryLoader

        active: false
        source: "qrc:/resources/qml/dialogs/room/RoomDirectory.qml"
        onLoaded: item.appRoot = timelineRoot
    }

    function openRoomDirectory() {
        if (!roomDirectoryLoader.active)
            roomDirectoryLoader.active = true;
        if (roomDirectoryLoader.item)
            roomDirectoryLoader.item.open();
    }
    AppShortcuts {
        componentCatalog: componentCatalog
        timelineRoot: rootTimeline
    }
    RootEventRouter {
        componentCatalog: componentCatalog
        timelineRoot: rootTimeline
    }
    SelfVerificationCoordinator {
    }
    Shortcut {
        sequence: "Escape"
        enabled: mainWindow.depth > 1
                 && mainWindow.currentItem
                 && mainWindow.currentItem.objectName === "userSettingsPage"
        // Escape first clears the settings search box; once it's empty,
        // Escape closes the settings page.
        onActivated: {
            if (mainWindow.currentItem.searchActive)
                mainWindow.currentItem.clearSearch();
            else
                mainWindow.pop();
        }
    }
    StackView {
        id: mainWindow
        objectName: "mainWindow"

        function popPage() { pop(); }

        property Transition popEnterOrg
        property Transition popExitOrg

        // for some reason direct bindings to a hidden StackView don't work, so manually store and restore here.
        property Transition pushEnterOrg
        property Transition pushExitOrg
        property Transition replaceEnterOrg
        property Transition replaceExitOrg

        function updateTrans() {
            pushEnter = Settings.uiMotionAnimationsEnabled ? pushEnterOrg : noopEnterTransition;
            pushExit = Settings.uiMotionAnimationsEnabled ? pushExitOrg : noopExitTransition;
            popEnter = Settings.uiMotionAnimationsEnabled ? popEnterOrg : noopEnterTransition;
            popExit = Settings.uiMotionAnimationsEnabled ? popExitOrg : noopExitTransition;
            replaceEnter = Settings.uiMotionAnimationsEnabled ? replaceEnterOrg : noopEnterTransition;
            replaceExit = Settings.uiMotionAnimationsEnabled ? replaceExitOrg : noopExitTransition;
        }

        function openUserSettingsPage(initialTab, scrollToSection) {
            if (mainWindow.currentItem && mainWindow.currentItem.objectName === "userSettingsPage") {
                if (initialTab !== undefined)
                    mainWindow.currentItem.currentTab = initialTab;
                if (scrollToSection)
                    mainWindow.currentItem.scrollToSection = scrollToSection;
                return;
            }
            var props = {};
            if (initialTab !== undefined)
                props.currentTab = initialTab;
            if (scrollToSection)
                props.scrollToSection = scrollToSection;
            if (Object.keys(props).length > 0)
                mainWindow.push(userSettingsPage, props);
            else
                mainWindow.push(userSettingsPage);
        }

        anchors.fill: parent
        initialItem: startupRestorePage

        Component.onCompleted: {
            pushEnterOrg = pushEnter;
            popEnterOrg = popEnter;
            replaceEnterOrg = replaceEnter;
            pushExitOrg = pushExit;
            popExitOrg = popExit;
            replaceExitOrg = replaceExit;
            updateTrans();
        }

        Transition {
            id: noopEnterTransition
            PropertyAnimation { property: "opacity"; to: 1; duration: 0 }
        }
        Transition {
            id: noopExitTransition
            PropertyAnimation { property: "opacity"; to: 0; duration: 0 }
        }
        Connections {
            function onUiMotionAnimationsEnabledChanged() {
                mainWindow.updateTrans();
            }

            target: Settings
        }

    }
    Component {
        id: startupRestorePage

        StartupRestorePage {
        }
    }
    Component {
        id: welcomePage

        WelcomePage {
        }
    }
    Component {
        id: newToMatrixPage

        NewToMatrixPage {
        }
    }
    Component {
        id: profileSwitcherPage

        ProfileSwitcherPage {
        }
    }
    Component {
        id: chatPage

        ChatPage {
            timelineRoot: rootTimeline
        }
    }
    Component {
        id: loginPage

        LoginPage {
            loginController: sharedLoginController
        }
    }
    Component {
        id: registerPage

        RegisterPage {
            registrationController: sharedRegistrationController
        }
    }
    Component {
        id: userSettingsPage

        UserSettingsPage {
        }
    }
    Snackbar {
        id: snackbar

        contentAreaItem: {
            const current = mainWindow.currentItem;
            if (current && current.notificationAreaItem)
                return current.notificationAreaItem;
            return current || timelineRoot;
        }
        avoidBottomItem: {
            const current = mainWindow.currentItem;
            if (current && current.notificationAvoidBottomItem)
                return current.notificationAvoidBottomItem;
            return null;
        }
    }
    Connections {
        target: ElementCall
        // Re-push liveness when a ring starts, so the controller learns the call
        // is live (and can later detect the caller cancelling).
        function onIncomingRingChanged() {
            timelineRoot.pushIncomingRingLiveness();
        }
    }
    // Feed the ringing call's live state (is anyone still in it?) to the
    // controller, so it can stop ringing when the caller cancels before we
    // answer. Sourced from Rooms.activeCalls, which tracks live calls per room.
    Connections {
        target: Rooms
        function onActiveCallsChanged() {
            timelineRoot.pushIncomingRingLiveness();
        }
    }
    function pushIncomingRingLiveness() {
        if (ElementCall.incomingRingActive && ElementCall.incomingRingRoomId.length > 0)
            ElementCall.updateRingLiveness(
                Rooms.activeCalls[ElementCall.incomingRingRoomId] !== undefined);
    }
    Connections {
        function onShowNotification(msg) {
            snackbar.showNotification(msg);
            console.log("New snack: " + msg);
        }
        function onShowNotificationWithActions(msg, actions) {
            snackbar.showNotificationWithActions(msg, actions);
            console.log("New snack with " + (actions ? actions.length : 0) + " action(s): " + msg);
        }
        function onSwitchToStartupRestorePage() {
            mainWindow.replace(null, startupRestorePage);
        }
        function onSwitchToChatPage() {
            mainWindow.replace(null, chatPage);
        }
        function onSwitchToLoginPage(error) {
            mainWindow.replace(null, loginPage, {
                    "error": error
                }, StackView.PopTransition);
        }
        function onSwitchToWelcomePage() {
            mainWindow.replace(null, welcomePage);
        }
        function onShowUserSettingsPageRequested() {
            mainWindow.openUserSettingsPage();
        }
        function onShowUserSettingsPageWithTabRequested(initialTab) {
            mainWindow.openUserSettingsPage(initialTab);
        }
        function onShowUserSettingsPageWithTabAndSectionRequested(initialTab, scrollToSection) {
            mainWindow.openUserSettingsPage(initialTab, scrollToSection);
        }
        function onShowProfileSwitcherPageRequested() {
            mainWindow.replace(null, profileSwitcherPage);
        }
        function onOpenRoomDirectoryRequested() {
            timelineRoot.openRoomDirectory();
        }

        target: MainWindow
    }
}
