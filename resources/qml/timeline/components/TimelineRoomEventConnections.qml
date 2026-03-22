// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

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

    function onSpecialEffectsTriggered(effectNames) {
        if (!Settings.timelineMediaEffectsEnabled)
            return;

        timelineView.shouldEffectsRun = true;
        timelineEffects.pulseEffects(effectNames);
        effectsTimer.interval = timelineEffects.durationForEffects(effectNames);
        effectsTimer.restart();
    }

    function onOpenReadReceiptsDialog(rr) {
        var component = Qt.createComponent(componentCatalog.timelineReadReceiptsDialog);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(dialogParent(), {
                    "readReceipts": rr,
                    "room": room
                });
            dialog.open();
            destroyDialogOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }

    function onShowRawMessageDialog(renderedRawMessage, rawMessageJson, rawMessageBody, rawMessageFormattedBody) {
        var component = Qt.createComponent(componentCatalog.timelineRawMessageDialog);
        if (component.status == Component.Ready) {
            var dialog = component.createObject(dialogParent(), {
                    "renderedRawMessage": renderedRawMessage,
                    "rawMessageJson": rawMessageJson,
                    "rawMessageBody": rawMessageBody,
                    "rawMessageFormattedBody": rawMessageFormattedBody
                });
            if (dialog.open != undefined)
                dialog.open();
            else
                dialog.show();
            destroyDialogOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }

    target: room
}
