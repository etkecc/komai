// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

#ifdef KOMAI_WITH_QML
#include <QQmlEngine>
#endif

namespace crypto {
Q_NAMESPACE
#ifdef KOMAI_WITH_QML
QML_NAMED_ELEMENT(Crypto)
#endif

enum Trust
{
    Unverified,
    MessageUnverified,
    TOFU,
    Verified,
};
Q_ENUM_NS(Trust)

/// Per-message shield state, mirroring matrix-sdk-common's `ShieldStateCode`
/// plus an explicit "no shield" entry for verified/clean messages. The old
/// `Trust` enum is a nheko-era 4-bucket summary used for per-user and
/// per-room aggregates; this richer enum is what the timeline indicator
/// consumes so each SDK code can surface its own tooltip and severity.
/// Values are grouped by severity tier so QML can `case`-fall-through.
enum MessageShield
{
    // No shield — the message is encrypted by a device we trust.
    ShieldNone,
    // Grey tier — authenticity caveats, no active impersonation signal.
    ShieldAuthenticityNotGuaranteed,
    ShieldUnknownDevice,
    ShieldUnsignedDevice,
    // Red tier — actively suspicious; demand user attention.
    ShieldSentInClear,
    ShieldUnverifiedIdentity,
    ShieldVerificationViolation,
    ShieldMismatchedSender,
};
Q_ENUM_NS(MessageShield)
}
