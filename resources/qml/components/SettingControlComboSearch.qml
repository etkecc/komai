// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import cc.etke.komai

KomaiSearchableComboBox {
    id: root

    required property int value
    required property var values

    signal activatedValueChanged(int index)

    model: values
    currentIndex: value
    onActivated: function(index) { activatedValueChanged(index) }
}
