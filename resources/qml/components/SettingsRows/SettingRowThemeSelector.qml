// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import cc.etke.komai

Item {
    id: root

    required property var model
    property bool leftAligned: false

    readonly property var safeThemeVariantValues:
        (model && model.themeVariantValues !== undefined) ? model.themeVariantValues : []
    readonly property int safeThemeVariantValue:
        (model && model.themeVariantValue !== undefined) ? model.themeVariantValue : 0
    readonly property var safeValues: (model && model.values !== undefined) ? model.values : []
    readonly property int safeValue: (model && model.value !== undefined) ? model.value : 0

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    Row {
        id: row
        anchors.left: parent.left
        spacing: root.leftAligned ? Math.max(1, Math.round(Komai.paddingSmall / 2)) : Komai.paddingSmall

        KomaiComboBox {
            id: variantCombo
            font.pointSize: Settings.uiFontSizePt
            model: safeThemeVariantValues
            currentIndex: safeThemeVariantValue
            onActivated: {
                if (!root.model)
                    return;
                if (currentIndex !== safeThemeVariantValue)
                    root.model.themeVariantValue = currentIndex;
            }
            implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
            wheelEnabled: activeFocus
        }

        KomaiComboBox {
            id: themeCombo
            font.pointSize: Settings.uiFontSizePt
            model: safeValues
            currentIndex: safeValue
            onActivated: {
                if (!root.model)
                    return;
                if (currentIndex >= 0 && currentIndex !== safeValue)
                    root.model.value = currentIndex;
            }
            implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
            wheelEnabled: activeFocus
        }
    }
}
