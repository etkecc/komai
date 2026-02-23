// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Loader {
    id: root

    required property var model
    property bool revealed: false

    sourceComponent: root.revealed ? revealedComponent : hiddenComponent

    Component {
        id: hiddenComponent
        Button {
            anchors.right: parent.right
            text: qsTr("Click to reveal")
            onClicked: root.revealed = true
        }
    }

    Component {
        id: revealedComponent
        RowLayout {
            spacing: Nheko.paddingSmall

            TextField {
                text: root.model.value
                readOnly: true
                Layout.preferredWidth: 350
            }

            ImageButton {
                id: copyBtn
                property bool copied: false
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
                ToolTip.visible: hovered
                ToolTip.text: copied ? qsTr("Copied!") : qsTr("Copy to clipboard")
                onClicked: {
                    Clipboard.text = root.model.value;
                    copied = true;
                    copyFeedbackTimer.start();
                }

                Timer {
                    id: copyFeedbackTimer
                    interval: 2000
                    onTriggered: copyBtn.copied = false
                }
            }
        }
    }
}
