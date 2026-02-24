// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import im.nheko

SpinBox {
    id: root

    required property var model

    readonly property double div: 100
    readonly property int decimals: 2

    anchors.right: parent.right
    font.pointSize: Settings.fontSize
    from: model.valueLowerBound * div
    to: model.valueUpperBound * div
    stepSize: model.valueStep * div
    value: model.value * div
    onValueModified: model.value = value / div
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
