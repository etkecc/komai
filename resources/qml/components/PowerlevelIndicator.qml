// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Image {
    id: root

    required property var powerlevel
    required property AbstractPermissions permissions
    property bool isCreator: false
    property color iconColor: palette.buttonText
    readonly property int permissionsRevision: permissions ? permissions.revision : 0

    readonly property bool isAdmin: {
        const _ = permissionsRevision;
        return permissions ? permissions.changeLevel(MtxEvent.PowerLevels) <= powerlevel : false;
    }
    readonly property bool isModerator: {
        const _ = permissionsRevision;
        return permissions ? permissions.redactLevel() <= powerlevel : false;
    }
    readonly property bool isDefault: {
        const _ = permissionsRevision;
        return permissions ? permissions.defaultLevel() <= powerlevel : false;
    }

    readonly property string roleName: {
        let pl = powerlevel.toLocaleString(Qt.locale(), "f", 0);
        if (isCreator)
            return qsTr("Creator");
        else if (isAdmin)
            return qsTr("Administrator (%1)").arg(pl);
        else if (isModerator)
            return qsTr("Moderator (%1)").arg(pl);
        else
            return qsTr("User (%1)").arg(pl);
    }

    readonly property string sourceUrl: {
        if (isAdmin || isCreator)
             return "image://colorimage/:/icons/icons/ui/ribbon_star.svg?";
        else if (isModerator)
            return "image://colorimage/:/icons/icons/ui/ribbon.svg?";
        else
            return "image://colorimage/:/icons/icons/ui/person.svg?";
    }

    sourceSize.width: width
    sourceSize.height: height
    source: sourceUrl + (ma.hovered ? palette.highlight : iconColor)
    readonly property string toolTipText: roleName

    KomaiToolTip {
        anchorItem: root
        anchorX: root.width / 2
        anchorY: root.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: root.toolTipText
        requestedVisible: ma.hovered && root.toolTipText.length > 0
    }

    HoverHandler {
        id: ma
    }
}
