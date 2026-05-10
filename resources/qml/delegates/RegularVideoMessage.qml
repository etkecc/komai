// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtMultimedia
import QtQuick
import QtQuick.Window
import cc.etke.komai
import QtQuick.Effects

Item {
    id: content

    property var roomAdapter: null
    required property double proportionalHeight
    required property int originalWidth
    required property int duration
    required property string thumbnailUrl
    required property string eventId
    required property string url
    required property string body
    required property string filename
    required property string filesize
    required property string mimetype

    property double divisor: EventDelegateChooser.isReply ? 10 : 4
    property int tempWidth: originalWidth < 1 ? 400 : originalWidth
    readonly property double safeProportionalHeight: proportionalHeight > 0 ? proportionalHeight : 0.75
    readonly property var roomContext: roomAdapter
        ? roomAdapter
        : (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)
    // Body (when it differs from filename) is rendered separately as a
    // media caption below the video, so don't pull it into the in-frame
    // info overlay as well.
    readonly property string mediaLabel: filename.length > 0 ? filename : body

    implicitWidth: Math.round(tempWidth * Math.min((timelineView.height / divisor) / (tempWidth * safeProportionalHeight), 1))
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: width * safeProportionalHeight

    MxcMedia {
        id: videoMedia

        roomm: roomContext
        eventId: content.eventId
        mimeTypeHint: content.mimetype
        videoOutput: videoOutput
        muted: true
        volume: 1.0
    }

    Rectangle {
        id: videoMask

        anchors.fill: videoContainer
        radius: 8
        layer.enabled: true
        visible: false
    }

    Rectangle {
        id: videoContainer

        color: palette.window
        width: parent.width
        height: parent.height
        radius: 8
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: videoMask
        }

        MouseArea {
            id: videoMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (!roomContext)
                    return;

                if (Settings.timelineMediaOpenVideosExternal) {
                    roomContext.openMedia(content.eventId);
                } else if (typeof roomContext.openMediaOverlay === "function"
                        && roomContext.openMediaOverlay(content.eventId)) {
                    return;
                } else if (roomContext.isActiveMatrixTimelineRoom === true) {
                    TimelineManager.openMediaOverlay(null,
                                                     content.url,
                                                     content.eventId,
                                                     content.originalWidth,
                                                     content.proportionalHeight,
                                                     MtxEvent.VideoMessage,
                                                     content.duration,
                                                     content.thumbnailUrl);
                } else {
                    TimelineManager.openMediaOverlayWithContext(
                        roomContext, content.url, content.eventId,
                        content.originalWidth, content.proportionalHeight,
                        MtxEvent.VideoMessage, content.duration, content.thumbnailUrl,
                        timeline, timelineView);
                }
            }
        }

        Image {
            id: videoThumb
            anchors.fill: parent
            source: content.thumbnailUrl
                ? ((roomContext && roomContext.isActiveMatrixTimelineRoom === true)
                    ? ("image://MxcImage/matrix-timeline:" + content.eventId + "?scale")
                    : (thumbnailUrl.replace("mxc://", "image://MxcImage/") + "?scale" + (roomContext ? "&room=" + roomContext.roomId : "")))
                : "image://colorimage/:/icons/icons/ui/video-file.svg?" + palette.windowText
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            sourceSize.width: Math.min(Screen.desktopAvailableWidth, content.tempWidth) * Screen.devicePixelRatio
            sourceSize.height: Math.min(Screen.desktopAvailableHeight, content.tempWidth * content.safeProportionalHeight) * Screen.devicePixelRatio

            VideoOutput {
                id: videoOutput

                visible: true
                clip: true
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
                orientation: videoMedia.orientation
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.3)

            Rectangle {
                id: playButton
                anchors.centerIn: parent
                width: Math.max(56, Math.min(parent.width, parent.height) * 0.3)
                height: width
                radius: width / 2
                color: videoMouseArea.containsMouse ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(0, 0, 0, 0.6)
                scale: videoMouseArea.containsMouse ? 1.08 : 1.0

                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on scale { NumberAnimation { duration: 150 } }

                Image {
                    anchors.centerIn: parent
                    width: parent.width * 0.45
                    height: width
                    source: "image://colorimage/:/icons/icons/ui/play-sign.svg?white"
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                }
            }
        }

        Item {
            id: videoInfoOverlay
            anchors.fill: parent

            Rectangle {
                width: parent.width
                height: videoInfoColumn.height
                anchors.bottom: parent.bottom
                color: palette.window
                opacity: 0.75
            }

            Column {
                id: videoInfoColumn
                width: parent.width
                anchors.bottom: parent.bottom

                Text {
                    width: parent.width
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                    text: content.mediaLabel
                    color: palette.text
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: content.filesize
                    color: palette.text
                    opacity: 0.7
                    font.pointSize: Qt.application.font.pointSize * 0.9
                }
            }
        }
    }
}
