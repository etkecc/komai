// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

TextField {
    id: root

    required property string textValue

    signal submitted(string text)

    text: textValue

    function applyText()
    {
        root.submitted(text.trim());
    }

    onEditingFinished: applyText()
    onAccepted: applyText()
    onActiveFocusChanged: if (!activeFocus)
        applyText()

    Component.onDestruction: applyText()
}
