// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Pane {
    id: timelineRoot
    property var activeImageOverlay: null
    property color overlayBackdropColor: Qt.rgba(0.2, 0.2, 0.2, 0.7)
    readonly property var rootTimeline: timelineRoot

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
    function showCatalogDialog(componentUrl, properties) {
        var dialog = createDialog(componentUrl, properties);
        if (!dialog)
            return null;
        dialog.show();
        destroyOnClose(dialog);
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
    function showForwardMessageDialog(room, eventId, timeline, timelineView) {
        if (!room || !eventId)
            return;

        var dialog = createDialog(componentCatalog.navigationForwardCompleterDialog, {
                "roomSource": room,
                "timelineSource": timeline ?? null,
                "timelineViewSource": timelineView ?? null,
                "showReplyPreview": !!timeline && !!timelineView
            });
        if (!dialog) {
            console.error("Failed to create ForwardCompleter object");
            return;
        }

        dialog.setMessageEventId(eventId);
        dialog.open();
        destroyOnClose(dialog);
    }

    //Timer {
    //    onTriggered: gc()
    //    interval: 1000
    //    running: true
    //    repeat: true
    //}
    function showAliasEditor(settings) {
        showCatalogDialog(componentCatalog.roomAliasEditorDialog, {
                "roomSettings": settings
            });
    }
    function showAllowedRoomsEditor(settings) {
        showCatalogDialog(componentCatalog.roomAllowedRoomsSettingsDialog, {
                "roomSettings": settings
            });
    }
    function showPLEditor(settings) {
        showCatalogDialog(componentCatalog.roomPowerLevelEditorDialog, {
                "roomSettings": settings
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

    }
    UserDirectoryModel {
        id: userDirectory

    }
    RoomDirectoryModel {
        id: publicRooms

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
    UiaCoordinator {
        timelineRoot: rootTimeline
        componentCatalog: componentCatalog
    }
    StackView {
        id: mainWindow

        property Transition popEnterOrg
        property Transition popExitOrg

        // for some reason direct bindings to a hidden StackView don't work, so manually store and restore here.
        property Transition pushEnterOrg
        property Transition pushExitOrg
        property Transition replaceEnterOrg
        property Transition replaceExitOrg

        function updateTrans() {
            pushEnter = Settings.uiMotionAnimationsEnabled ? pushEnterOrg : reducedMotionNoopTransition;
            pushExit = Settings.uiMotionAnimationsEnabled ? pushExitOrg : reducedMotionNoopTransition;
            popEnter = Settings.uiMotionAnimationsEnabled ? popEnterOrg : reducedMotionNoopTransition;
            popExit = Settings.uiMotionAnimationsEnabled ? popExitOrg : reducedMotionNoopTransition;
            replaceEnter = Settings.uiMotionAnimationsEnabled ? replaceEnterOrg : reducedMotionNoopTransition;
            replaceExit = Settings.uiMotionAnimationsEnabled ? replaceExitOrg : reducedMotionNoopTransition;
        }

        function openUserSettingsPage() {
            if (mainWindow.currentItem && mainWindow.currentItem.objectName === "userSettingsPage")
                return;
            mainWindow.push(userSettingsPage);
        }

        anchors.fill: parent
        initialItem: welcomePage

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
            id: reducedMotionNoopTransition
        }
        Connections {
            function onUiMotionAnimationsEnabledChanged() {
                mainWindow.updateTrans();
            }

            target: Settings
        }
    }
    Component {
        id: welcomePage

        WelcomePage {
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
        }
    }
    Component {
        id: registerPage

        RegisterPage {
        }
    }
    Component {
        id: userSettingsPage

        UserSettingsPage {
        }
    }
    Snackbar {
        id: snackbar

    }
    Connections {
        function onShowNotification(msg) {
            snackbar.showNotification(msg);
            console.log("New snack: " + msg);
        }
        function onSwitchToChatPage() {
            mainWindow.replace(null, chatPage);
        }
        function onSwitchToLoginPage(error) {
            mainWindow.replace(welcomePage, {}, loginPage, {
                    "error": error
                }, StackView.PopTransition);
        }
        function onShowUserSettingsPageRequested() {
            mainWindow.openUserSettingsPage();
        }

        target: MainWindow
    }
}
