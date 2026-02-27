// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../dialogs"
import "../dialogs/timeline"
import "../pages"
import "../ui"
import "../components/encryption" as Encryption
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import im.nheko

Pane {
    id: timelineRoot
    property var activeImageOverlay: null
    property color overlayBackdropColor: Qt.rgba(0.2, 0.2, 0.2, 0.7)

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
    function showForwardMessageDialog(room, eventId, timeline, timelineView) {
        if (!room || !eventId)
            return;

        var component = Qt.createComponent(componentCatalog.forwardCompleter);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(timelineRoot, {
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
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }

    //Timer {
    //    onTriggered: gc()
    //    interval: 1000
    //    running: true
    //    repeat: true
    //}
    function showAliasEditor(settings) {
        var component = Qt.createComponent(componentCatalog.roomAliasEditorDialog);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(timelineRoot, {
                    "roomSettings": settings
                });
            dialog.show();
            destroyOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }
    function showAllowedRoomsEditor(settings) {
        var component = Qt.createComponent(componentCatalog.roomAllowedRoomsSettingsDialog);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(timelineRoot, {
                    "roomSettings": settings
                });
            dialog.show();
            destroyOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }
    function showPLEditor(settings) {
        var component = Qt.createComponent(componentCatalog.powerLevelEditorDialog);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(timelineRoot, {
                    "roomSettings": settings
                });
            dialog.show();
            destroyOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }
    function showSpacePLApplyPrompt(settings, editingModel) {
        var component = Qt.createComponent(componentCatalog.powerLevelSpacesApplyDialog);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(timelineRoot, {
                    "roomSettings": settings,
                    "editingModel": editingModel
                });
            dialog.show();
            destroyOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
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
    Component {
        id: readReceiptsDialog

        ReadReceipts {
        }
    }
    Shortcut {
        sequence: StandardKey.Quit

        onActivated: Qt.quit()
    }
    Shortcut {
        sequences: ["Escape"]
        context: Qt.ApplicationShortcut
        enabled: !!timelineRoot.activeImageOverlay && timelineRoot.activeImageOverlay.visible

        onActivated: timelineRoot.activeImageOverlay.close()
        onActivatedAmbiguously: timelineRoot.activeImageOverlay.close()
    }
    Shortcut {
        sequence: "Ctrl+K"

        onActivated: {
            var component = Qt.createComponent(componentCatalog.quickSwitcher);
            if (component.status == Component.Ready) {
                var quickSwitch = component.createObject(timelineRoot);
                quickSwitch.open();
                destroyOnClosed(quickSwitch);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
    }
    Shortcut {
        sequences: [StandardKey.ZoomIn, "Ctrl+Plus", "Ctrl+Equal", "Ctrl+Shift+Equal"]
        context: Qt.ApplicationShortcut

        onActivated: timelineRoot.adjustFontSize(1)
    }
    Shortcut {
        sequences: [StandardKey.ZoomOut, "Ctrl+Minus", "Ctrl+Underscore"]
        context: Qt.ApplicationShortcut

        onActivated: timelineRoot.adjustFontSize(-1)
    }
    Shortcut {
        // Add alternative shortcut, because sometimes Alt+A is stolen by the TextEdit
        sequences: ["Alt+A", "Ctrl+Shift+A"]

        onActivated: Rooms.nextRoomWithActivity()
    }
    Shortcut {
        sequences: ["Ctrl+Down", "Ctrl+PgDown"]

        onActivated: Rooms.nextRoom()
    }
    Shortcut {
        sequences: ["Ctrl+Up", "Ctrl+PgUp"]

        onActivated: Rooms.previousRoom()
    }
    Connections {
        function onOpenJoinRoomDialog() {
            var component = Qt.createComponent(componentCatalog.roomJoinDialog);
            if (component.status == Component.Ready) {
                var dialog = component.createObject(timelineRoot);
                dialog.show();
                destroyOnClose(dialog);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
        function onOpenLogoutDialog() {
            var component = Qt.createComponent(componentCatalog.logoutDialog);
            if (component.status == Component.Ready) {
                var dialog = component.createObject(timelineRoot);
                dialog.open();
                destroyOnClose(dialog);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
        function onShowRoomJoinPrompt(summary) {
            var component = Qt.createComponent(componentCatalog.roomConfirmJoinDialog);
            if (component.status == Component.Ready) {
                var dialog = component.createObject(timelineRoot, {
                        "summary": summary
                    });
                dialog.show();
                destroyOnClose(dialog);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }

        target: Nheko
    }
    Connections {
        function onNewDeviceVerificationRequest(flow) {
            var component = Qt.createComponent(componentCatalog.deviceVerificationDialog);
            if (component.status == Component.Ready) {
                var dialog = component.createObject(timelineRoot, {
                        "flow": flow
                    });
                dialog.show();
                destroyOnClose(dialog);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }

        target: VerificationManager
    }
    Connections {
        function onOpenInviteUsersDialog(invitees) {
            var component = Qt.createComponent(componentCatalog.roomInviteDialog);
            if (component.status == Component.Ready) {
                var dialog = component.createObject(timelineRoot, {
                        "invitees": invitees
                    });
                dialog.show();
                destroyOnClose(dialog);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
        function onOpenLeaveRoomDialog(roomid, reason) {
            var component = Qt.createComponent(componentCatalog.roomLeaveDialog);
            if (component.status == Component.Ready) {
                var dialog = component.createObject(timelineRoot, {
                        "roomId": roomid,
                        "reason": reason
                    });
                dialog.open();
                destroyOnClose(dialog);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
        function onOpenProfile(profile) {
            var component = Qt.createComponent(componentCatalog.userProfileDialog);
            if (component.status == Component.Ready) {
                var userProfile = component.createObject(timelineRoot, {
                        "profile": profile
                    });
                userProfile.show();
                destroyOnClose(userProfile);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
        function onOpenRoomMembersDialog(members, room) {
            var component = Qt.createComponent(componentCatalog.roomMembersDialog);
            if (component.status == Component.Ready) {
                var membersDialog = component.createObject(timelineRoot, {
                        "members": members,
                        "room": room
                    });
                membersDialog.show();
                destroyOnClose(membersDialog);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
        function onOpenRoomSettingsDialog(settings) {
            var component = Qt.createComponent(componentCatalog.roomSettingsDialog);
            if (component.status == Component.Ready) {
                var roomSettings = component.createObject(timelineRoot, {
                        "roomSettings": settings
                    });
                roomSettings.show();
                destroyOnClose(roomSettings);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
        function onShowImageOverlay(room, eventId, url, originalWidth, proportionalHeight, timeline, timelineView) {
            var component = Qt.createComponent(componentCatalog.imageOverlayDialog);
            if (component.status == Component.Ready) {
                var dialog = component.createObject(timelineRoot, {
                        "room": room,
                        "eventId": eventId,
                        "url": url,
                        "originalWidth": originalWidth ?? 0,
                        "proportionalHeight": proportionalHeight ?? 0,
                        "timelineContext": timeline ?? null,
                        "timelineViewContext": timelineView ?? null,
                        "popupParent": timelineRoot,
                        "modalOverlayColor": timelineRoot.overlayBackdropColor,
                        "actionButtonColor": "white",
                        "actionButtonHoverColor": "white",
                        "actionBarColor": Qt.rgba(0, 0, 0, 0.35),
                        "actionButtonHoverBackgroundColor": Qt.rgba(0, 0, 0, 0.45)
                    });
                timelineRoot.activeImageOverlay = dialog;
                dialog.visibleChanged.connect(() => {
                    if (!dialog.visible && timelineRoot.activeImageOverlay === dialog)
                        timelineRoot.activeImageOverlay = null;
                });
                dialog.showFullScreen();
                dialog.raise();
                dialog.requestActivate();
                destroyOnClose(dialog);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }
        function onShowImagePackSettings(room, packlist) {
            var component = Qt.createComponent(componentCatalog.imagePackSettingsDialog);
            if (component.status == Component.Ready) {
                var packSet = component.createObject(timelineRoot, {
                        "room": room,
                        "packlist": packlist
                    });
                packSet.show();
                destroyOnClose(packSet);
            } else {
                console.error("Failed to create component: " + component.errorString());
            }
        }

        target: TimelineManager
    }
    Connections {
        function onNewInviteState() {
            if (CallManager.haveCallInvite && !Settings.uiInputMode && Settings.callsLegacyEnabled) {
                var component = Qt.createComponent(componentCatalog.callInviteDialog);
                if (component.status == Component.Ready) {
                    var dialog = component.createObject(timelineRoot);
                    dialog.open();
                    destroyOnClose(dialog);
                } else {
                    console.error("Failed to create component: " + component.errorString());
                }
            }
        }

        target: CallManager
    }
    Encryption.SelfVerificationCoordinator {
    }
    UiaCoordinator {
        timelineRoot: timelineRoot
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
