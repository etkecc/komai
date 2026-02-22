// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Item {
    required property var model

    RowLayout {
        anchors.right: parent.right
        spacing: Nheko.paddingSmall

        ComboBox {
            id: variantCombo
            model: model.themeVariantValues
            currentIndex: model.themeVariantValue
            onActivated: {
                if (currentIndex !== model.themeVariantValue)
                    model.themeVariantValue = currentIndex;
            }
            implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
            wheelEnabled: activeFocus
        }

        ComboBox {
            id: themeCombo
            visible: variantCombo.currentIndex !== 2
            model: model.values
            currentIndex: model.value
            onActivated: {
                if (currentIndex >= 0 && currentIndex !== model.value)
                    model.value = currentIndex;
            }
            implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
            wheelEnabled: activeFocus
        }
    }
}
