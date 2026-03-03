// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

Image {
    id: stateImg

    property bool encrypted: false
    property bool hovered: ma.hovered
    property string sourceUrl: {
        if (!encrypted)
            return "image://colorimage/" + unencryptedIcon + "?";
        switch (trust) {
        case Crypto.Verified:
            return "image://colorimage/:/icons/icons/ui/shield-regular-checkmark.svg?";
        case Crypto.TOFU:
            return "image://colorimage/:/icons/icons/ui/shield-regular.svg?";
        case Crypto.Unverified:
        case Crypto.MessageUnverified:
            return "image://colorimage/:/icons/icons/ui/shield-regular-exclamation-mark.svg?";
        default:
            return "image://colorimage/:/icons/icons/ui/shield-regular-cross.svg?";
        }
    }
    property int trust: Crypto.Unverified
    property color unencryptedColor: Komai.theme.error
    property color unencryptedHoverColor: unencryptedColor
    property string unencryptedIcon: ":/icons/icons/ui/shield-regular-cross.svg"

    ToolTip.text: {
        if (!encrypted)
            return qsTr("This message is not encrypted!");
        switch (trust) {
        case Crypto.Verified:
            return qsTr("Encrypted by a verified device");
        case Crypto.TOFU:
            return qsTr("Encrypted by an unverified device, but you have trusted that user so far.");
        case Crypto.MessageUnverified:
            return qsTr("Key is from an untrusted source, possibly forwarded from another user or the online key backup. For this reason we can't verify who sent the message.");
        default:
            return qsTr("Encrypted by an unverified device.");
        }
    }
    ToolTip.visible: stateImg.hovered
    fillMode: Image.PreserveAspectFit
    height: 16
    source: {
        if (encrypted) {
            switch (trust) {
            case Crypto.Verified:
                return sourceUrl + Komai.theme.success;
            case Crypto.TOFU:
                return sourceUrl + palette.buttonText;
            default:
                return sourceUrl + Komai.theme.error;
            }
        } else {
            return sourceUrl + (stateImg.hovered ? unencryptedHoverColor : unencryptedColor);
        }
    }
    width: 16

    HoverHandler {
        id: ma

    }
}
