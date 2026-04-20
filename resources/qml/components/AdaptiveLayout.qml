// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Container {
    id: container

    property int splitterGrabMargin: Komai.paddingSmall
    property Component handle
    property Component handleToucharea

    Component.onCompleted: {
        for (var i = 0; i < count - 1; i++) {
            let handle_ = handle.createObject(contentChildren[i]);
            let split_ = handleToucharea.createObject(contentChildren[i]);
            contentChildren[i].width = Qt.binding(function() {
                return split_.calculatedWidth;
            });
            contentChildren[i].splitterWidth = Qt.binding(function() {
                return handle_.width;
            });
        }
        contentChildren[count - 1].width = Qt.binding(function() {
            var w = container.width;
            for (var i = 0; i < count - 1; i++) {
                if (contentChildren[i].width)
                    w = w - contentChildren[i].width;
            }
            return w;
        });
        contentChildren[count - 1].splitterWidth = 0;
        for (var i = 0; i < count; i++) {
            contentChildren[i].height = Qt.binding(function() {
                return container.height;
            });
            contentChildren[i].children[0].height = Qt.binding(function() {
                return container.height;
            });
        }
    }

    handle: Rectangle {
        z: 3
        color: Komai.theme.separator
        height: container.height
        width: visible ? 1 : 0
        anchors.right: parent.right
    }

    handleToucharea: Item {
        id: splitter

        property int maximumWidth: parent.maximumWidth
        property int collapsedWidth: parent.collapsedWidth
        property int snapUpperWidth: parent.snapUpperWidth
        property int calculatedWidth: visible ? x : 0

        height: container.height
        width: 1
        x: parent.preferredWidth
        z: 3

        KomaiCursorShape {
            height: parent.height
            width: container.splitterGrabMargin * 2
            x: -container.splitterGrabMargin
            cursorShape: Qt.SizeHorCursor
        }

        DragHandler {
            id: dragHandler

            xAxis.enabled: true
            yAxis.enabled: false
            xAxis.minimum: splitter.collapsedWidth
            xAxis.maximum: splitter.maximumWidth
            margin: container.splitterGrabMargin
            grabPermissions: PointerHandler.CanTakeOverFromAnything | PointerHandler.ApprovesTakeOverByHandlersOfSameType
            onActiveChanged: {
                if (!active) {
                    let finalX = splitter.calculatedWidth;
                    if (splitter.snapUpperWidth > splitter.collapsedWidth
                        && finalX > splitter.collapsedWidth
                        && finalX < splitter.snapUpperWidth) {
                        const midpoint = (splitter.collapsedWidth + splitter.snapUpperWidth) / 2;
                        finalX = finalX < midpoint ? splitter.collapsedWidth : splitter.snapUpperWidth;
                    }
                    splitter.x = finalX;
                    splitter.parent.preferredWidth = finalX;
                }
            }
        }

        HoverHandler {
            margin: container.splitterGrabMargin
        }

    }

    contentItem: ListView {
        id: view

        model: container.contentModel
        orientation: ListView.Horizontal
        interactive: false
        currentIndex: 0
        boundsBehavior: Flickable.StopAtBounds
    }

}
