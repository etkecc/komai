// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import im.nheko
import "../../dialogs"

Button {
    text: qsTr("Open Profile Settings")
    icon.source: "qrc:/icons/icons/ui/person.svg"

    onClicked: {
        Nheko.updateUserProfile();
        var component = Qt.createComponent("qrc:/resources/qml/dialogs/UserProfile.qml");
        if (component.status == Component.Ready) {
            var userProfile = component.createObject(timelineRoot, {
                "profile": Nheko.currentUser
            });
            userProfile.show();
            timelineRoot.destroyOnClose(userProfile);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }
}
