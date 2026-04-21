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
    // Per-message shield mode: when `useShield` is true, rendering switches
    // from the legacy 4-bucket `Crypto.Trust` to the richer
    // `Crypto.MessageShield` code surfaced directly by matrix-sdk-ui.
    // Room-header and member-list callers leave `useShield` at its default
    // and keep using `trust`.
    property int shield: Crypto.ShieldNone
    property bool useShield: false

    readonly property bool shieldIsGreyTier: useShield
        && (shield === Crypto.ShieldAuthenticityNotGuaranteed
            || shield === Crypto.ShieldUnknownDevice
            || shield === Crypto.ShieldUnsignedDevice)
    readonly property bool shieldIsRedTier: useShield
        && (shield === Crypto.ShieldSentInClear
            || shield === Crypto.ShieldUnverifiedIdentity
            || shield === Crypto.ShieldVerificationViolation
            || shield === Crypto.ShieldMismatchedSender)
    readonly property bool shieldIsNone: useShield && shield === Crypto.ShieldNone
    // `encrypted` gates the overall clear/unencrypted warning; the shield
    // codes only apply to events we consider encrypted. `ShieldSentInClear`
    // is the one shield code that still needs to render when `encrypted`
    // is false (it's the reason the event reads as unencrypted), so let
    // the shield path take over in that case too.
    readonly property bool shieldDrivesRendering: useShield
        && (encrypted || shield === Crypto.ShieldSentInClear)

    property string sourceUrl: {
        if (shieldDrivesRendering) {
            if (shieldIsNone)
                return "image://colorimage/:/icons/icons/ui/shield-regular-checkmark.svg?";
            if (shieldIsGreyTier)
                return "image://colorimage/:/icons/icons/ui/shield-regular.svg?";
            if (shield === Crypto.ShieldSentInClear)
                return "image://colorimage/:/icons/icons/ui/shield-regular-cross.svg?";
            // Remaining red-tier codes share the exclamation shield.
            return "image://colorimage/:/icons/icons/ui/shield-regular-exclamation-mark.svg?";
        }
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

    // Tooltip strings follow matrix-sdk-common's canonical shield messages
    // (see crates/matrix-sdk-common/src/deserialized_responses.rs). Keeping
    // them close to the SDK wording means future spec clarifications can
    // migrate over without a translation drift.
    property string toolTipText: {
        if (shieldDrivesRendering) {
            switch (shield) {
            case Crypto.ShieldNone:
                return qsTr("Encrypted by a verified device.");
            case Crypto.ShieldAuthenticityNotGuaranteed:
                return qsTr("The authenticity of this encrypted message can't be guaranteed on this device.");
            case Crypto.ShieldUnknownDevice:
                return qsTr("Encrypted by an unknown or deleted device.");
            case Crypto.ShieldUnsignedDevice:
                return qsTr("Encrypted by a device not verified by its owner.");
            case Crypto.ShieldSentInClear:
                return qsTr("Not encrypted.");
            case Crypto.ShieldUnverifiedIdentity:
                return qsTr("Encrypted by an unverified user.");
            case Crypto.ShieldVerificationViolation:
                return qsTr("Encrypted by a previously-verified user who is no longer verified.");
            case Crypto.ShieldMismatchedSender:
                return qsTr("The sender of the event does not match the owner of the device that created the Megolm session.");
            }
        }
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
    sourceSize.width: width
    sourceSize.height: height
    source: {
        if (shieldDrivesRendering) {
            const useHover = stateImg.hovered && encryptedHoverEnabled;
            if (shieldIsNone)
                return sourceUrl + (useHover ? encryptedHoverColor : Komai.theme.success);
            if (shieldIsGreyTier)
                return sourceUrl + (useHover ? encryptedHoverColor : palette.buttonText);
            return sourceUrl + (useHover ? encryptedHoverColor : Komai.theme.error);
        }
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
