// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

SpinBox {
    id: root

    anchors.right: parent.right
    from: model.valueLowerBound
    to: model.valueUpperBound
    stepSize: model.valueStep
    value: model.value
    onValueChanged: model.value = value
    editable: true
    wheelEnabled: activeFocus

    required property var model
}
