// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import cc.etke.komai

KomaiSpinBox {
    id: root

    required property var model

    readonly property double div: 100
    readonly property int decimals: 2

    anchors.right: parent?.right
    font.pointSize: Settings.uiFontSizePt
    // Guard against `model` going null while the delegate is being torn down
    // (e.g. when the settings search filter removes this row): unguarded
    // `model.X` produces "Cannot read property of null" warnings on every
    // keystroke in the search field.
    from: (model?.valueLowerBound ?? 0) * div
    to: (model?.valueUpperBound ?? 0) * div
    stepSize: (model?.valueStep ?? 1) * div
    value: (model?.value ?? 0) * div
    onValueModified: {
        if (!root.model)
            return;
        const nextValue = value / root.div;
        if (root.model.value !== nextValue)
            root.model.value = nextValue;
    }
    editable: true
    wheelEnabled: activeFocus

    property real realValue: value / div

    validator: DoubleValidator {
        bottom: Math.min(root.from / root.div, root.to / root.div)
        top: Math.max(root.from / root.div, root.to / root.div)
    }

    textFromValue: function(value, locale) {
        return Number(value / root.div).toLocaleString(locale, 'f', root.decimals)
    }

    valueFromText: function(text, locale) {
        return Number.fromLocaleString(locale, text) * root.div
    }
}
