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
    property var dialogHost: null
    required property var componentCatalog

    function dialogParent() {
        return dialogHost || timelineView;
    }

    function destroyDialogOnClose(dialog) {
        if (!dialog)
            return;
        if (dialogHost && dialogHost.destroyOnClose) {
            dialogHost.destroyOnClose(dialog);
            return;
        }
        if (dialog.closing != undefined) {
            dialog.closing.connect(() => dialog.destroy(1000));
        } else if (dialog.aboutToHide != undefined) {
            dialog.aboutToHide.connect(() => dialog.destroy(1000));
        }
    }

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
        var component = Qt.createComponent(componentCatalog.timelineReadReceiptsDialog);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(dialogParent(), {
                    "readReceipts": rr,
                    "room": room
                });
            dialog.show();
            destroyDialogOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
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
            var dialog = component.createObject(dialogParent(), {
                    "rawMessage": rawMessage
                });
            dialog.show();
            destroyDialogOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }

    target: room
}
