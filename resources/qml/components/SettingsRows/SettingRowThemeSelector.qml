// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

    implicitHeight: row.implicitHeight

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: root.leftAligned ? Math.max(1, Math.round(Komai.paddingSmall / 2)) : Komai.paddingSmall

        SegmentedButton {
            id: variantSegment
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            currentIndex: root.safeThemeVariantValue
            model: (root.safeThemeVariantValues || []).map(function(v) { return { text: v }; })
            onActivated: function(index) {
                if (!root.model)
                    return;
                if (index !== root.safeThemeVariantValue)
                    root.model.themeVariantValue = index;
            }
        }

        KomaiComboBox {
            id: themeCombo
            Layout.alignment: Qt.AlignVCenter
            font.pointSize: Settings.uiFontSizePt
            model: root.safeValues
            // Re-apply currentIndex after the model array changes — assigning
            // a new model can leave ComboBox.currentIndex pointing at a stale
            // slot (the wrong theme name shows next to the new variant).
            onModelChanged: currentIndex = root.safeValue
            Component.onCompleted: currentIndex = root.safeValue
            onActivated: {
                if (!root.model)
                    return;
                if (currentIndex >= 0 && currentIndex !== root.safeValue)
                    root.model.value = currentIndex;
            }
            implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
            wheelEnabled: activeFocus

            Connections {
                target: root
                function onSafeValueChanged() {
                    if (themeCombo.currentIndex !== root.safeValue)
                        themeCombo.currentIndex = root.safeValue;
                }
            }
        }
    }
}
