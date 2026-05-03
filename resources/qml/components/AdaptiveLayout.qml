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
    readonly property bool splittersOnLeft: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
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
            contentChildren[i].splitterOnLeft = Qt.binding(function() {
                return container.splittersOnLeft;
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
        x: container.splittersOnLeft ? 0 : parent.width - width
        LayoutMirroring.enabled: false
    }

    handleToucharea: Item {
        id: splitter

        property int maximumWidth: parent.maximumWidth
        property int collapsedWidth: parent.collapsedWidth
        property int snapUpperWidth: parent.snapUpperWidth
        property bool dragging: false
        property int dragStartWidth: parent.preferredWidth
        property int dragWidth: parent.preferredWidth
        property int calculatedWidth: visible ? (dragging ? dragWidth : parent.preferredWidth) : 0

        function clampedWidth(width) {
            return Math.max(collapsedWidth, Math.min(maximumWidth, width));
        }

        height: container.height
        width: 1
        x: container.splittersOnLeft ? 0 : parent.width - width
        z: 3
        LayoutMirroring.enabled: false

        KomaiCursorShape {
            height: parent.height
            width: container.splitterGrabMargin * 2
            x: -container.splitterGrabMargin
            cursorShape: Qt.SizeHorCursor
        }

        DragHandler {
            id: dragHandler

            target: null
            xAxis.enabled: true
            yAxis.enabled: false
            margin: container.splitterGrabMargin
            grabPermissions: PointerHandler.CanTakeOverFromAnything | PointerHandler.ApprovesTakeOverByHandlersOfSameType
            onTranslationChanged: {
                if (active) {
                    const delta = container.splittersOnLeft ? -translation.x : translation.x;
                    splitter.dragWidth = splitter.clampedWidth(splitter.dragStartWidth + delta);
                }
            }
            onActiveChanged: {
                if (active) {
                    splitter.dragStartWidth = splitter.calculatedWidth;
                    splitter.dragWidth = splitter.dragStartWidth;
                    splitter.dragging = true;
                } else {
                    let finalX = splitter.calculatedWidth;
                    if (splitter.snapUpperWidth > splitter.collapsedWidth
                        && finalX > splitter.collapsedWidth
                        && finalX < splitter.snapUpperWidth) {
                        const midpoint = (splitter.collapsedWidth + splitter.snapUpperWidth) / 2;
                        finalX = finalX < midpoint ? splitter.collapsedWidth : splitter.snapUpperWidth;
                    }
                    splitter.dragging = false;
                    splitter.parent.preferredWidth = finalX;
                    splitter.dragStartWidth = finalX;
                    splitter.dragWidth = finalX;
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
