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
            timelineRoot.openCatalogDialog(componentCatalog.verificationDeviceDialog, {
                    "flow": flow
                });
        }

        target: VerificationManager
    }
    Connections {
        function onOpenInviteUsersDialog(invitees) {
            timelineRoot.openCatalogDialog(componentCatalog.roomInviteDialog, {
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
            timelineRoot.openCatalogDialog(componentCatalog.userProfileDialog, {
                    "profile": profile,
                    "appRoot": timelineRoot
                });
        }
        function onOpenRoomInfoDialog(settings, members, room, initialTab) {
            timelineRoot.openCatalogDialog(componentCatalog.roomInfoDialog, {
                    "roomSettings": settings,
                    "members": members,
                    "room": room,
                    "appRoot": timelineRoot,
                    "initialTab": initialTab
                });
        }
        function onShowMediaOverlay(room, eventId, url, originalWidth, proportionalHeight,
                                    mediaType, duration, thumbnailUrl, timeline, timelineView) {
            var dialog = timelineRoot.createDialog(componentCatalog.mediaOverlayDialog, {
                    "room": room,
                    "eventId": eventId,
                    "url": url,
                    "originalWidth": originalWidth ?? 0,
                    "proportionalHeight": proportionalHeight ?? 0,
                    "mediaType": mediaType ?? -1,
                    "mediaDuration": duration ?? 0,
                    "thumbnailUrl": thumbnailUrl ?? "",
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

            timelineRoot.activeMediaOverlay = dialog;
            dialog.visibleChanged.connect(() => {
                if (!dialog.visible && timelineRoot.activeMediaOverlay === dialog)
                    timelineRoot.activeMediaOverlay = null;
            });
            // Use maximized mode: fills the screen but leaves the taskbar
            // visible so the user can access system volume controls.
            // Explicit x/y positioning is unreliable on Wayland.
            dialog.showMaximized();
            dialog.raise();
            dialog.requestActivate();
            timelineRoot.destroyOnClose(dialog);
        }
        function onShowImagePackSettings(packlist, canCreateRoomPack) {
            timelineRoot.openCatalogDialog(componentCatalog.mediaImagePackSettingsDialog, {
                    "packlist": packlist,
                    "canCreateRoomPack": !!canCreateRoomPack
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
