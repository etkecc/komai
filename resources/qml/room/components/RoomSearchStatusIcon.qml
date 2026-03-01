// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: searchIcon

    required property var room
    required property bool filteringInProgress
    required property int topBarAvatarSize

    property bool _rawLoading: (room && room.paginationInProgress) || filteringInProgress
    property bool isLoading: _rawLoading || searchLoadingHoldTimer.running

    on_RawLoadingChanged: {
        if (_rawLoading)
            searchLoadingHoldTimer.stop();
        else
            searchLoadingHoldTimer.start();
    }

    Timer {
        id: searchLoadingHoldTimer
        interval: 200
    }

    Layout.preferredHeight: topBarAvatarSize
    Layout.preferredWidth: topBarAvatarSize

    // Composite rendered as one unit for the desaturation effect
    Item {
        id: searchIconContent

        anchors.fill: parent
        visible: false
        layer.enabled: true

        Image {
            anchors.fill: parent
            source: "qrc:/logos/komai.svg"
            sourceSize.width: width * 2
            sourceSize.height: height * 2
            fillMode: Image.PreserveAspectFit
        }
        Rectangle {
            id: searchBadge

            property int badgeSize: Math.round(topBarAvatarSize * 0.55)
            property int iconSize: Math.round(badgeSize * 0.69)

            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.bottomMargin: -2
            anchors.rightMargin: -2
            width: badgeSize
            height: badgeSize
            radius: Math.round(badgeSize * 0.25)
            color: palette.alternateBase

            transform: Translate { id: badgeTranslate; x: 0 }

            SequentialAnimation {
                loops: Animation.Infinite
                running: searchIcon.isLoading && Settings.uiMotionAnimationsEnabled

                ParallelAnimation {
                    NumberAnimation {
                        target: badgeTranslate; property: "x"
                        from: 0; to: 3
                        duration: 300; easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        target: searchBadge; property: "scale"
                        from: 1.0; to: 1.3
                        duration: 300; easing.type: Easing.InOutQuad
                    }
                }
                ParallelAnimation {
                    NumberAnimation {
                        target: badgeTranslate; property: "x"
                        from: 3; to: -3
                        duration: 600; easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        target: searchBadge; property: "scale"
                        from: 1.3; to: 1.3
                        duration: 600
                    }
                }
                ParallelAnimation {
                    NumberAnimation {
                        target: badgeTranslate; property: "x"
                        from: -3; to: 0
                        duration: 300; easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        target: searchBadge; property: "scale"
                        from: 1.3; to: 1.0
                        duration: 300; easing.type: Easing.InOutQuad
                    }
                }

                onRunningChanged: {
                    if (!running) {
                        badgeTranslate.x = 0;
                        searchBadge.scale = 1.0;
                    }
                }
            }

            Image {
                anchors.centerIn: parent
                source: "image://colorimage/:/icons/icons/ui/search.svg?" + (searchIcon.isLoading ? palette.highlight : palette.text)
                sourceSize.width: searchBadge.iconSize
                sourceSize.height: searchBadge.iconSize
                width: searchBadge.iconSize
                height: searchBadge.iconSize
            }
        }
    }
    MultiEffect {
        id: searchEffect

        anchors.fill: parent
        source: searchIconContent
        saturation: searchIcon.isLoading && !Settings.uiMotionAnimationsEnabled ? 0.0 : -1.0

        SequentialAnimation {
            loops: Animation.Infinite
            running: searchIcon.isLoading && Settings.uiMotionAnimationsEnabled

            NumberAnimation {
                target: searchEffect
                property: "saturation"
                from: -1.0
                to: 0.0
                duration: 800
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: searchEffect
                property: "saturation"
                from: 0.0
                to: -1.0
                duration: 800
                easing.type: Easing.InOutQuad
            }

            onRunningChanged: {
                if (!running) {
                    searchEffect.saturation = Qt.binding(function() {
                        return (searchIcon.isLoading && !Settings.uiMotionAnimationsEnabled) ? 0.0 : -1.0;
                    });
                }
            }
        }
    }
}
