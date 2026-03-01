// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

RowLayout {
    Label {
        Layout.alignment: Qt.AlignVCenter
        Layout.margins: Komai.paddingSmall
        text: qsTr("Theme")
        color: palette.text
    }

    ComboBox {
        id: variantCombo
        model: [qsTr("Light"), qsTr("Dark"), qsTr("System")]
        currentIndex: Settings.themeVariantIndex()
        onActivated: function(index) {
            Settings.setThemeVariantByIndex(index)
        }
        implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
        wheelEnabled: activeFocus
    }

    ComboBox {
        id: themeCombo
        visible: variantCombo.currentIndex !== 2
        model: Settings.themeNamesForCurrentVariant()
        currentIndex: Settings.themeIndexInCurrentVariant()
        onActivated: function(index) {
            Settings.setThemeByVariantIndex(index)
        }
        implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted
        wheelEnabled: activeFocus
    }

    Connections {
        target: Settings

        function onUiThemeSlugChanged() {
            variantCombo.currentIndex = Settings.themeVariantIndex()
            themeCombo.model = Settings.themeNamesForCurrentVariant()
            themeCombo.currentIndex = Settings.themeIndexInCurrentVariant()
        }
    }

    Item {
        Layout.preferredWidth: Komai.paddingLarge
    }

    ToggleButton {
        id: animationsToggle
        Layout.alignment: Qt.AlignVCenter
        checked: Settings.uiMotionAnimationsEnabled
        onCheckedChanged: Settings.uiMotionAnimationsEnabled = checked
    }

    Label {
        Layout.alignment: Qt.AlignVCenter
        Layout.margins: Komai.paddingSmall
        text: qsTr("Enable animations")
        color: palette.text

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: animationsToggle.toggle()
        }

        HoverHandler {
            id: hovered
        }
        ToolTip.visible: hovered.hovered
        ToolTip.text: qsTr("Komai uses animations in several places to improve visual feedback. Disable them if they make you feel unwell.")
        ToolTip.delay: Komai.tooltipDelay
    }
}
