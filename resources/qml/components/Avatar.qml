// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Qt5Compat.GraphicalEffects
import cc.etke.komai 1.0

AbstractButton {
    id: avatar

    property alias color: bg.color
    property bool crop: true
    property string displayName
    property color fallbackBorderColor: palette.mid
    property int fallbackBorderWidth: 1
    property bool mirrorPresenceIndicator: false
    readonly property int frameWidth: Settings.uiAvatarsCircular ? Math.max(1, Math.round(width) - (Math.round(width) % 2)) : width
    readonly property int frameHeight: Settings.uiAvatarsCircular ? Math.max(1, Math.round(height) - (Math.round(height) % 2)) : height
    readonly property real avatarRadius: Settings.uiAvatarsCircular ? frameHeight / 2 : frameHeight / 8
    property string roomid
    property bool showFallbackBorder: img.status != Image.Ready
    property color textColor: palette.text
    property string url
    property string userid

    // Logical thumbnail side-length for the MxcImage fetch.
    // max(displaySize, listIconSize) so small avatars reuse the standard cache
    // entry and large avatars get full resolution.  DPR scaling is handled
    // entirely in C++ (MxcImageProvider applies QScreen DPR).
    readonly property int _mxcThumbSidePx: Math.max(Math.max(Math.round(width), Math.round(height)), Komai.iconSize)
    readonly property real _devicePixelRatio: {
        if (Window.window && Window.window.screen)
            return Window.window.screen.devicePixelRatio || 1;
        return Screen.devicePixelRatio || 1;
    }
    readonly property bool _isBundledRasterAvatar: avatar.url.startsWith(':/preview-avatars/')
        || avatar.url.startsWith('qrc:/preview-avatars/')
    readonly property int _rasterThumbSidePx: Math.max(1, Math.round(avatar._mxcThumbSidePx * avatar._devicePixelRatio))

    height: 48
    width: 48

    background: Rectangle {
        id: bg

        anchors.centerIn: parent
        width: avatar.frameWidth
        height: avatar.frameHeight
        color: "transparent"
        border.width: avatar.showFallbackBorder ? avatar.fallbackBorderWidth : 0
        border.color: avatar.fallbackBorderColor
        radius: avatar.avatarRadius
    }

    readonly property color userColorHint: {
        if (avatar.userid === "")
            return palette.text
        if (avatar.roomid !== "")
            return TimelineManager.roomUserColor(avatar.roomid, avatar.userid,
                       palette.base, Settings.timelineUserColorCodingPolicy)
        return TimelineManager.userColor(avatar.userid, palette.base)
    }

    Item {
        id: avatarClipper

        anchors.fill: bg
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: avatarClipper.width
                height: avatarClipper.height
                radius: avatar.avatarRadius
            }
        }

        Image {
            id: defaultAvatar

            anchors.fill: parent
            source: "image://default-avatar/"
                    + (avatar.userid !== "" ? avatar.userid : avatar.roomid)
                    + "?radius=" + (Settings.uiAvatarsCircular ? 100 : 25)
                    + "&displayName=" + encodeURIComponent(avatar.displayName || "")
                    + "&color=" + avatar.userColorHint.toString().substring(1)
                    + "&style=" + Settings.uiAvatarsDefaultAvatarStyle
                    + "&_v=" + Settings.uiAvatarsDefaultAvatarStyle
            visible: img.status != Image.Ready
        }
        Image {
            id: img

            anchors.fill: parent
            // Bundled resource SVGs (e.g. ":/icons/icons/ui/world.svg") load
            // synchronously so that colour changes on hover don't flicker.
            asynchronous: !avatar.url.startsWith(':/')
            fillMode: avatar.crop ? Image.PreserveAspectCrop : Image.PreserveAspectFit
            source: if (avatar.url.startsWith('image://colorimage')) {
                return avatar.url + "&radius=" + (Settings.uiAvatarsCircular ? 100 : 25) + ((avatar.crop) ? "" : "&scale");
            } else if (avatar.url.startsWith('image://')) {
                return avatar.url + "?radius=" + (Settings.uiAvatarsCircular ? 100 : 25)
                    + ((avatar.crop) ? "" : "&scale")
                    + "&avatarSize=" + avatar._mxcThumbSidePx;
            } else if (avatar.url.startsWith(':/logos/') || avatar.url.startsWith('qrc:/logos/')
                       || avatar.url.startsWith(':/preview-avatars/') || avatar.url.startsWith('qrc:/preview-avatars/')) {
                // Keep branded logos and bundled avatar images un-tinted.
                return avatar.url;
            } else if (avatar.url.startsWith(':/')) {
                return "image://colorimage/" + avatar.url + "?" + avatar.textColor;
            } else {
                return "";
            }
            // Request DPR-aware source pixels for bundled raster avatars. MxcImage
            // already applies device-pixel-ratio handling in C++.
            sourceSize: Qt.size(avatar._isBundledRasterAvatar ? avatar._rasterThumbSidePx : avatar._mxcThumbSidePx,
                                avatar._isBundledRasterAvatar ? avatar._rasterThumbSidePx : avatar._mxcThumbSidePx)
        }
    }
    Rectangle {
        id: onlineIndicator

        function updatePresence() {
            switch (Presence.userPresence(avatar.userid)) {
            case "online":
                return Komai.theme.online;
            case "unavailable":
                return Komai.theme.unavailable;
            case "offline":
            default:
                // return "#a82353" don't show anything if offline, since it is confusing, if presence is disabled
                return "transparent";
            }
        }

        anchors.bottom: bg.bottom
        anchors.left: avatar.mirrorPresenceIndicator ? bg.left : undefined
        anchors.right: avatar.mirrorPresenceIndicator ? undefined : bg.right
        color: updatePresence()
        height: bg.height / 6
        radius: Settings.uiAvatarsCircular ? Math.floor(height / 2) : height / 8
        visible: !!avatar.userid
        width: height

        Connections {
            function onPresenceChanged(id) {
                if (id == avatar.userid)
                    onlineIndicator.color = onlineIndicator.updatePresence();
            }

            target: Presence
        }
    }
    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
}
