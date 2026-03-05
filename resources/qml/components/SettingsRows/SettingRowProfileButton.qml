// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import "../../ui"
import cc.etke.komai

KomaiButton {
    ComponentCatalog {
        id: componentCatalog
    }

    text: qsTr("Open Profile Settings")
    icon.source: "qrc:/icons/icons/ui/person.svg"

    onClicked: {
        Komai.updateUserProfile();
        var component = Qt.createComponent(componentCatalog.userProfileDialog);
        if (component.status == Component.Ready) {
            var userProfile = component.createObject(timelineRoot, {
                "profile": Komai.currentUser
            });
            userProfile.show();
            timelineRoot.destroyOnClose(userProfile);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }
}
