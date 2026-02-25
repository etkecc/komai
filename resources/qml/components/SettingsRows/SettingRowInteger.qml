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

    anchors.right: parent.right
    font.pointSize: Settings.uiFontSizePt
    from: model.valueLowerBound
    to: model.valueUpperBound
    stepSize: model.valueStep
    value: model.value
    onValueModified: {
        if (!root.model)
            return;
        if (root.model.value !== value)
            root.model.value = value;
    }
    editable: true
    wheelEnabled: activeFocus

    required property var model
}
