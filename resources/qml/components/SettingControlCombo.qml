// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import im.nheko

ComboBox {
    id: root

    required property int value
    required property var values

    signal activatedValueChanged(int index)

    font.pointSize: Settings.uiFontSizePt
    model: values
    currentIndex: value
    onActivated: activatedValueChanged(currentIndex)
    implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
    wheelEnabled: activeFocus
}
