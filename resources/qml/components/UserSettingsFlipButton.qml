// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

AvatarSettingsFlipButton {
    id: control

    property var profile: Komai.currentUser

    avatarDisplayName: profile ? profile.displayName : ""
    avatarUrl: (profile ? profile.avatarUrl : "").replace("mxc://", "image://MxcImage/")
    avatarUserId: profile ? profile.userid : ""
    toolTipText: (profile ? profile.displayName : "") + "\n" + (profile ? profile.userid : "")
}
