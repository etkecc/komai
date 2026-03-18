// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
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
    property color encryptedHoverColor: palette.brightText
    property bool encryptedHoverEnabled: false
    property string unencryptedIcon: ":/icons/icons/ui/shield-regular-cross.svg"

    property string toolTipText: {
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

    KomaiToolTip {
        anchorItem: stateImg
        anchorX: stateImg.width / 2
        anchorY: stateImg.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: stateImg.toolTipText
        requestedVisible: stateImg.hovered && stateImg.toolTipText.length > 0
    }
    fillMode: Image.PreserveAspectFit
    height: 16
    source: {
        if (encrypted) {
            const useHover = stateImg.hovered && encryptedHoverEnabled;
            switch (trust) {
            case Crypto.Verified:
                return sourceUrl + (useHover ? encryptedHoverColor : Komai.theme.success);
            case Crypto.TOFU:
                return sourceUrl + (useHover ? encryptedHoverColor : palette.buttonText);
            default:
                return sourceUrl + (useHover ? encryptedHoverColor : Komai.theme.error);
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
