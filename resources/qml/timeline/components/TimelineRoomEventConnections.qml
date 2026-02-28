// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import im.nheko

Connections {
    id: root

    required property var room
    required property var timelineView
    required property var timelineEffects
    required property var effectsTimer
    required property var readReceiptsDialog
    required property var timelineRoot
    required property var componentCatalog

    function onConfetti() {
        if (!Settings.timelineMediaEffectsEnabled)
            return;
        timelineView.shouldEffectsRun = true;
        timelineEffects.pulseConfetti();
        room.markSpecialEffectsDone();
    }

    function onConfettiDone() {
        if (!Settings.timelineMediaEffectsEnabled)
            return;
        effectsTimer.restart();
    }

    function onOpenReadReceiptsDialog(rr) {
        var dialog = readReceiptsDialog.createObject(timelineRoot, {
                "readReceipts": rr,
                "room": room
            });
        dialog.show();
        timelineRoot.destroyOnClose(dialog);
    }

    function onRainfall() {
        if (!Settings.timelineMediaEffectsEnabled)
            return;
        timelineView.shouldEffectsRun = true;
        timelineEffects.pulseRainfall();
        room.markSpecialEffectsDone();
    }

    function onRainfallDone() {
        if (!Settings.timelineMediaEffectsEnabled)
            return;
        effectsTimer.restart();
    }

    function onShowRawMessageDialog(rawMessage) {
        var component = Qt.createComponent(componentCatalog.timelineRawMessageDialog);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(timelineRoot, {
                    "rawMessage": rawMessage
                });
            dialog.show();
            timelineRoot.destroyOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }

    target: room
}
