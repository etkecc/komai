// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../ui"
import cc.etke.komai 1.0

MatrixText {
    required property string typeString

    text: qsTr("unimplemented event: ") + typeString
//    width: parent.width
    color: palette.inactive.text
}
