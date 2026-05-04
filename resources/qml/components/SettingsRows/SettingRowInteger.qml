// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import cc.etke.komai

KomaiSpinBox {
    id: root

    anchors.right: parent?.right
    font.pointSize: Settings.uiFontSizePt
    // Guard against `model` going null while the delegate is being torn down
    // (search-filter row removal): unguarded `model.X` produces "Cannot read
    // property of null" warnings on every keystroke in the search field.
    from: model?.valueLowerBound ?? 0
    to: model?.valueUpperBound ?? 0
    stepSize: model?.valueStep ?? 1
    value: model?.value ?? 0
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
