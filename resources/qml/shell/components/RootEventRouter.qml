// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    required property var componentCatalog
    required property var timelineRoot

    Connections {
        function onOpenJoinRoomDialog() {
            timelineRoot.openCatalogDialog(componentCatalog.roomJoinDialog);
        }
        function onOpenLogoutDialog() {
            timelineRoot.openCatalogDialog(componentCatalog.accountLogoutDialog);
        }
        function onShowRoomJoinPrompt(summary) {
            timelineRoot.openCatalogDialog(componentCatalog.roomConfirmJoinDialog, {
                    "summary": summary
                });
        }

        target: Komai
    }
    Connections {
        function onNewDeviceVerificationRequest(flow) {
            timelineRoot.showCatalogDialog(componentCatalog.verificationDeviceDialog, {
                    "flow": flow
                });
        }

        target: VerificationManager
    }
    Connections {
        function onOpenInviteUsersDialog(invitees) {
            timelineRoot.showCatalogDialog(componentCatalog.roomInviteDialog, {
                    "invitees": invitees
                });
        }
        function onOpenLeaveRoomDialog(roomid, reason) {
            timelineRoot.openCatalogDialog(componentCatalog.roomLeaveDialog, {
                    "roomId": roomid,
                    "reason": reason
                });
        }
        function onOpenProfile(profile) {
            timelineRoot.showCatalogDialog(componentCatalog.userProfileDialog, {
                    "profile": profile
                });
        }
        function onOpenRoomMembersDialog(members, room) {
            timelineRoot.showCatalogDialog(componentCatalog.roomMembersDialog, {
                    "members": members,
                    "room": room
                });
        }
        function onOpenRoomSettingsDialog(settings) {
            timelineRoot.showCatalogDialog(componentCatalog.roomSettingsDialog, {
                    "roomSettings": settings
                });
        }
        function onShowImageOverlay(room,
                                    eventId,
                                    url,
                                    originalWidth,
                                    proportionalHeight,
                                    timeline,
                                    timelineView) {
            var dialog = timelineRoot.createDialog(componentCatalog.mediaImageOverlayDialog, {
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
            if (!dialog)
                return;

            timelineRoot.activeImageOverlay = dialog;
            dialog.visibleChanged.connect(() => {
                if (!dialog.visible && timelineRoot.activeImageOverlay === dialog)
                    timelineRoot.activeImageOverlay = null;
            });
            dialog.showFullScreen();
            dialog.raise();
            dialog.requestActivate();
            timelineRoot.destroyOnClose(dialog);
        }
        function onShowImagePackSettings(room, packlist) {
            timelineRoot.showCatalogDialog(componentCatalog.mediaImagePackSettingsDialog, {
                    "room": room,
                    "packlist": packlist
                });
        }

        target: TimelineManager
    }
    Connections {
        function onNewInviteState() {
            if (CallManager.haveCallInvite && !Settings.uiInputMode && Settings.callsLegacyEnabled) {
                timelineRoot.openCatalogDialog(componentCatalog.voipCallInviteDialog);
            }
        }

        target: CallManager
    }
}
