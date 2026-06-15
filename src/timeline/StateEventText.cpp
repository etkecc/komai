// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StateEventText.h"

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include <QCoreApplication>

namespace StateEventText {

// Translation-context shim. lupdate is a parser, not a preprocessor, and
// won't expand a user-defined one-arg TR() macro -- it only recognises a
// fixed set of names (tr, qsTr, QCoreApplication::translate,
// Q_DECLARE_TR_FUNCTIONS, QT_*_NOOP, ...). Q_DECLARE_TR_FUNCTIONS(StateEventText)
// emits a static inline tr() that calls
// QCoreApplication::translate("StateEventText", ...), and lupdate uses the
// macro's argument as the extraction context. Tr::tr("...") below behaves
// identically at runtime to a hand-written
// QCoreApplication::translate("StateEventText", "...") call, but its
// literals actually land in the .ts catalogues.
class Tr
{
    Q_DECLARE_TR_FUNCTIONS(StateEventText)
};

// ── Helpers ─────────────────────────────────────────────────────────

static QString
formatUser(const QString &displayName, const QString &userId)
{
    if (displayName.isEmpty() || displayName == userId)
        return userId;
    return Tr::tr("%1 (%2)").arg(displayName, userId);
}

// ── Membership changes ──────────────────────────────────────────────

static QString
translateMembershipChange(const MatrixTimelineItem &item)
{
    const auto user      = formatUser(item.stateEventTargetUser, item.stateEventTargetUserId);
    const auto sender    = formatUser(item.senderDisplayName, item.senderId);
    const auto &reason   = item.stateEventReason;
    const bool hasSender = item.stateEventHasSender;
    const bool hasReason = !reason.isEmpty();
    const auto &kind     = item.membershipChangeKind;

    // Each combination of (hasSender, hasReason) is a distinct full sentence
    // so that translators can reorder all parts freely.

    if (kind == QStringLiteral("joined"))
        return Tr::tr("%1 joined the room").arg(user);

    if (kind == QStringLiteral("left")) {
        if (hasReason)
            return Tr::tr("%1 left the room: %2").arg(user, reason);
        return Tr::tr("%1 left the room").arg(user);
    }

    if (kind == QStringLiteral("banned")) {
        if (hasSender && hasReason)
            return Tr::tr("%1 was banned by %2: %3").arg(user, sender, reason);
        if (hasSender)
            return Tr::tr("%1 was banned by %2").arg(user, sender);
        if (hasReason)
            return Tr::tr("%1 was banned: %2").arg(user, reason);
        return Tr::tr("%1 was banned").arg(user);
    }

    if (kind == QStringLiteral("unbanned")) {
        if (hasSender)
            return Tr::tr("%1 was unbanned by %2").arg(user, sender);
        return Tr::tr("%1 was unbanned").arg(user);
    }

    if (kind == QStringLiteral("kicked")) {
        if (hasSender && hasReason)
            return Tr::tr("%1 was kicked by %2: %3").arg(user, sender, reason);
        if (hasSender)
            return Tr::tr("%1 was kicked by %2").arg(user, sender);
        if (hasReason)
            return Tr::tr("%1 was kicked: %2").arg(user, reason);
        return Tr::tr("%1 was kicked").arg(user);
    }

    if (kind == QStringLiteral("invited")) {
        if (hasSender)
            return Tr::tr("%1 was invited by %2").arg(user, sender);
        return Tr::tr("%1 was invited").arg(user);
    }

    if (kind == QStringLiteral("kicked_and_banned")) {
        if (hasSender && hasReason)
            return Tr::tr("%1 was kicked and banned by %2: %3").arg(user, sender, reason);
        if (hasSender)
            return Tr::tr("%1 was kicked and banned by %2").arg(user, sender);
        if (hasReason)
            return Tr::tr("%1 was kicked and banned: %2").arg(user, reason);
        return Tr::tr("%1 was kicked and banned").arg(user);
    }

    if (kind == QStringLiteral("invitation_accepted"))
        return Tr::tr("%1 accepted the invite").arg(user);

    if (kind == QStringLiteral("invitation_rejected"))
        return Tr::tr("%1 rejected the invite").arg(user);

    if (kind == QStringLiteral("invitation_revoked")) {
        if (hasSender)
            return Tr::tr("%1's invite was revoked by %2").arg(user, sender);
        return Tr::tr("%1's invite was revoked").arg(user);
    }

    if (kind == QStringLiteral("knocked"))
        return Tr::tr("%1 requested to join").arg(user);

    if (kind == QStringLiteral("knock_accepted")) {
        if (hasSender)
            return Tr::tr("%1's knock was accepted by %2").arg(user, sender);
        return Tr::tr("%1's knock was accepted").arg(user);
    }

    if (kind == QStringLiteral("knock_retracted"))
        return Tr::tr("%1 withdrew the join request").arg(user);

    if (kind == QStringLiteral("knock_denied")) {
        if (hasSender)
            return Tr::tr("%1's join request was denied by %2").arg(user, sender);
        return Tr::tr("%1's join request was denied").arg(user);
    }

    if (kind == QStringLiteral("redacted"))
        return Tr::tr("Redacted membership event for %1").arg(user);

    // none / error / not_implemented
    return Tr::tr("Membership updated for %1").arg(user);
}

// ── Profile changes ─────────────────────────────────────────────────

static QString
translateProfileChange(const MatrixTimelineItem &item)
{
    const auto user    = formatUser(item.stateEventTargetUser, item.stateEventTargetUserId);
    const auto &detail = item.stateEventDetail;

    // membershipChangeKind is reused to carry the profile-change sub-kind.
    const auto &subKind = item.membershipChangeKind;

    if (subKind == QStringLiteral("displayname") && !detail.isEmpty())
        return Tr::tr("%1 is now known as %2").arg(user, detail);

    if (subKind == QStringLiteral("displayname_removed"))
        return Tr::tr("%1 removed their display name").arg(user);

    if (subKind == QStringLiteral("avatar"))
        return Tr::tr("%1 changed their avatar").arg(user);

    return Tr::tr("%1 updated their profile").arg(user);
}

// ── Room state changes ──────────────────────────────────────────────

static QString
translateRoomName(const MatrixTimelineItem &item)
{
    const auto sender  = formatUser(item.senderDisplayName, item.senderId);
    const auto &detail = item.stateEventDetail;

    if (!detail.isEmpty())
        return Tr::tr("%1 changed the room name to: %2").arg(sender, detail);
    return Tr::tr("%1 removed the room name").arg(sender);
}

static QString
translateRoomTopic(const MatrixTimelineItem &item)
{
    const auto sender  = formatUser(item.senderDisplayName, item.senderId);
    const auto &detail = item.stateEventDetail;

    if (!detail.isEmpty())
        return Tr::tr("%1 changed the topic to: %2").arg(sender, detail);
    return Tr::tr("%1 removed the topic").arg(sender);
}

static QString
translateJoinRules(const MatrixTimelineItem &item)
{
    const auto sender  = formatUser(item.senderDisplayName, item.senderId);
    const auto &detail = item.stateEventDetail;

    if (detail == QStringLiteral("invite"))
        return Tr::tr("%1 changed the room access rules to invite-only").arg(sender);
    if (detail == QStringLiteral("knock"))
        return Tr::tr("%1 changed the room access rules to knock-to-join").arg(sender);
    if (detail == QStringLiteral("public"))
        return Tr::tr("%1 changed the room access rules to public").arg(sender);
    if (detail == QStringLiteral("private"))
        return Tr::tr("%1 changed the room access rules to private").arg(sender);
    if (detail == QStringLiteral("restricted"))
        return Tr::tr("%1 changed the room access rules to restricted").arg(sender);
    if (detail == QStringLiteral("knock_restricted"))
        return Tr::tr("%1 changed the room access rules to knock (restricted)").arg(sender);

    return Tr::tr("%1 changed the room access rules").arg(sender);
}

static QString
translateHistoryVisibility(const MatrixTimelineItem &item)
{
    const auto sender  = formatUser(item.senderDisplayName, item.senderId);
    const auto &detail = item.stateEventDetail;

    if (detail == QStringLiteral("invited"))
        return Tr::tr("%1 changed the room history visibility to visible since invite").arg(sender);
    if (detail == QStringLiteral("joined"))
        return Tr::tr("%1 changed the room history visibility to visible since join").arg(sender);
    if (detail == QStringLiteral("shared"))
        return Tr::tr("%1 changed the room history visibility to shared").arg(sender);
    if (detail == QStringLiteral("world_readable"))
        return Tr::tr("%1 changed the room history visibility to world-readable").arg(sender);

    return Tr::tr("%1 changed the room history visibility").arg(sender);
}

static QString
translateGuestAccess(const MatrixTimelineItem &item)
{
    const auto sender  = formatUser(item.senderDisplayName, item.senderId);
    const auto &detail = item.stateEventDetail;

    if (detail == QStringLiteral("can_join"))
        return Tr::tr("%1 changed the room guest access to allowed").arg(sender);
    if (detail == QStringLiteral("forbidden"))
        return Tr::tr("%1 changed the room guest access to forbidden").arg(sender);

    return Tr::tr("%1 changed the room guest access").arg(sender);
}

static QString
powerLevelName(int64_t level)
{
    if (level == 0)
        return Tr::tr("Default (%1)").arg(level);
    if (level == 50)
        return Tr::tr("Moderator (%1)").arg(level);
    if (level == 100)
        return Tr::tr("Administrator (%1)").arg(level);
    return Tr::tr("Custom (%1)").arg(level);
}

static QString
translatePowerLevels(const MatrixTimelineItem &item)
{
    const auto sender   = formatUser(item.senderDisplayName, item.senderId);
    const auto &changes = item.powerLevelChanges;

    if (changes.empty())
        return Tr::tr("%1 changed the room permissions").arg(sender);

    QStringList lines;
    for (const auto &change : changes) {
        lines.append(Tr::tr("%1 changed the power level of %2 from %3 to %4")
                       .arg(sender,
                            change.userId,
                            powerLevelName(change.oldLevel),
                            powerLevelName(change.newLevel)));
    }
    return lines.join(QStringLiteral("\n"));
}

static constexpr int serverAclMaxDetailedChanges = 10;

static QString
translateServerAcl(const MatrixTimelineItem &item)
{
    const auto sender  = formatUser(item.senderDisplayName, item.senderId);
    const auto &change = item.serverAclChange;

    if (change.isEmpty() || change.totalChanges() > serverAclMaxDetailedChanges)
        return Tr::tr("%1 changed which servers are allowed in this room").arg(sender);

    QStringList lines;

    for (const auto &server : change.deniedAdded)
        lines.append(Tr::tr("%1 blocked servers matching %2").arg(sender, server));
    for (const auto &server : change.deniedRemoved)
        lines.append(Tr::tr("%1 unblocked servers matching %2").arg(sender, server));
    for (const auto &server : change.allowedAdded)
        lines.append(Tr::tr("%1 allowed servers matching %2").arg(sender, server));
    for (const auto &server : change.allowedRemoved)
        lines.append(Tr::tr("%1 disallowed servers matching %2").arg(sender, server));

    if (change.ipLiteralsChange == 1)
        lines.append(Tr::tr("%1 allowed connections from IP literal servers").arg(sender));
    else if (change.ipLiteralsChange == 2)
        lines.append(Tr::tr("%1 blocked connections from IP literal servers").arg(sender));

    return lines.join(QStringLiteral("\n"));
}

static QString
translateOtherState(const MatrixTimelineItem &item)
{
    const auto sender     = formatUser(item.senderDisplayName, item.senderId);
    const auto &eventType = item.matrixEventType;

    if (eventType == QStringLiteral("m.room.name"))
        return translateRoomName(item);
    if (eventType == QStringLiteral("m.room.topic"))
        return translateRoomTopic(item);
    if (eventType == QStringLiteral("m.room.avatar"))
        return Tr::tr("%1 changed the room avatar").arg(sender);
    if (eventType == QStringLiteral("m.room.encryption"))
        return Tr::tr("%1 enabled end-to-end encryption").arg(sender);
    if (eventType == QStringLiteral("m.room.pinned_events"))
        return Tr::tr("%1 changed the pinned messages").arg(sender);
    if (eventType == QStringLiteral("m.room.power_levels"))
        return translatePowerLevels(item);
    if (eventType == QStringLiteral("m.room.join_rules"))
        return translateJoinRules(item);
    if (eventType == QStringLiteral("m.room.history_visibility"))
        return translateHistoryVisibility(item);
    if (eventType == QStringLiteral("m.room.guest_access"))
        return translateGuestAccess(item);
    if (eventType == QStringLiteral("m.room.canonical_alias"))
        return Tr::tr("%1 changed the addresses for this room").arg(sender);
    if (eventType == QStringLiteral("m.room.tombstone"))
        return Tr::tr("%1 replaced this room").arg(sender);
    if (eventType == QStringLiteral("m.room.server_acl"))
        return translateServerAcl(item);
    if (eventType == QStringLiteral("m.room.create"))
        return Tr::tr("%1 created and configured the room").arg(sender);
    if (eventType == QStringLiteral("m.space.parent"))
        return Tr::tr("%1 changed the parent communities for this room").arg(sender);
    if (eventType == QStringLiteral("m.space.child"))
        return Tr::tr("%1 changed a child room of this space").arg(sender);
    if (eventType == QStringLiteral("m.policy.rule.room") ||
        eventType == QStringLiteral("m.policy.rule.user") ||
        eventType == QStringLiteral("m.policy.rule.server"))
        return Tr::tr("%1 updated a moderation policy rule").arg(sender);

    return Tr::tr("%1 changed unknown state event %2").arg(sender, eventType);
}

// ── Event type labels ───────────────────────────────────────────────

QString
eventTypeLabel(const QString &itemKind, const QString & /*matrixEventType*/)
{
    if (itemKind == QStringLiteral("redacted"))
        return Tr::tr("Deleted message");
    if (itemKind == QStringLiteral("unable_to_decrypt"))
        return Tr::tr("[Unable to decrypt message]");
    if (itemKind == QStringLiteral("failed_to_parse_message_like"))
        return Tr::tr("[Unreadable message event]");
    if (itemKind == QStringLiteral("failed_to_parse_state"))
        return Tr::tr("[Unreadable state event]");
    if (itemKind == QStringLiteral("call_invite"))
        return Tr::tr("[Call invite]");
    if (itemKind == QStringLiteral("rtc_notification"))
        return Tr::tr("Started a call");
    if (itemKind == QStringLiteral("poll"))
        return Tr::tr("[Poll]");
    if (itemKind == QStringLiteral("sticker"))
        return Tr::tr("[Sticker]");
    if (itemKind == QStringLiteral("reaction"))
        return Tr::tr("Reactions updated");
    if (itemKind == QStringLiteral("other_message"))
        return Tr::tr("[Unsupported message event]");
    if (itemKind == QStringLiteral("unknown_message"))
        return Tr::tr("[Unsupported message event]");

    return {};
}

// ── Public API ──────────────────────────────────────────────────────

QString
translate(const MatrixTimelineItem &item)
{
    if (item.itemKind == QStringLiteral("membership_change"))
        return translateMembershipChange(item);
    if (item.itemKind == QStringLiteral("profile_change"))
        return translateProfileChange(item);
    if (item.itemKind == QStringLiteral("other_state"))
        return translateOtherState(item);

    // For non-state event types that have translatable labels
    // (redacted, unable_to_decrypt, call_invite, etc.)
    const auto label = eventTypeLabel(item.itemKind, item.matrixEventType);
    if (!label.isEmpty())
        return label;

    // Catch-all for any unrecognised state-like event kind that Rust may
    // introduce in the future.  Guarantees we never return an empty string
    // for something the timeline treats as a state event.
    if (!item.senderDisplayName.isEmpty())
        return Tr::tr("Room state changed by %1")
          .arg(formatUser(item.senderDisplayName, item.senderId));

    return {};
}

// ── Notification body translation ───────────────────────────────────

QString
translateNotificationBody(const MatrixNotificationItem &notification)
{
    const auto &kind = notification.notificationKind;

    // Invite notifications have no body from Rust -- provide translated text.
    if (kind == QStringLiteral("invite"))
        return Tr::tr("Invited you to join this room");

    // For event-type kinds that have translatable labels, use eventTypeLabel().
    // This covers: redacted, unable_to_decrypt, poll, call_invite, sticker,
    // reaction, other_message, unknown_message, etc.
    const auto label = eventTypeLabel(kind, {});
    if (!label.isEmpty())
        return label;

    // For membership/state events shown in notifications, provide a generic label.
    if (kind == QStringLiteral("membership_change"))
        return Tr::tr("[Membership change]");
    if (kind == QStringLiteral("other_state"))
        return Tr::tr("[Room state changed]");

    // Content-bearing kinds (text, image, video, etc.) -- use original body.
    return notification.plainBody;
}

// ── Room-list last-message preview ──────────────────────────────────

QString
translateRoomListPreview(const QString &kind, const QString &body)
{
    // Event-type labels (redacted, poll, sticker, etc.)
    const auto label = eventTypeLabel(kind, {});
    if (!label.isEmpty())
        return label;

    // State event kinds that need generic labels
    if (kind == QStringLiteral("membership_change"))
        return Tr::tr("[Membership change]");
    if (kind == QStringLiteral("profile_change"))
        return Tr::tr("[Profile updated]");
    if (kind == QStringLiteral("other_state"))
        return Tr::tr("[Room state changed]");

    // Content-bearing kinds -- use original body as-is
    return body;
}

// ── Auth / registration error translation ───────────────────────────
//
// Known constant error strings from Rust auth.rs and registration.rs.
// When changing error strings in those files, update the mappings here.

namespace {

// Try to split "Prefix: detail" and translate just the prefix. Returns
// empty string if the error does not start with the prefix.
//
// `prefix` is used both for the prefix-match (as QLatin1String for cheap
// startsWith) and as the translation source key (Tr::tr lookup), so call
// sites can wrap it in QT_TRANSLATE_NOOP("StateEventText", ...) to make
// lupdate extract the literal under the StateEventText context that
// Tr::tr resolves at runtime.
QString
translateErrorPrefix(const QString &error, const char *prefix)
{
    const auto prefixLatin = QLatin1String(prefix);
    if (!error.startsWith(prefixLatin))
        return {};

    const auto detail = error.mid(prefixLatin.size()).trimmed();
    if (detail.isEmpty())
        return Tr::tr(prefix);

    return QStringLiteral("%1 %2").arg(Tr::tr(prefix), detail);
}

} // anonymous namespace

QString
translateAuthError(const QString &error)
{
    // ── Constant strings (exact match) ──

    if (error ==
        QLatin1String("Received malformed response. Make sure the homeserver domain is valid."))
        return Tr::tr("Received malformed response. Make sure the homeserver domain is valid.");
    if (error == QLatin1String("Autodiscovery failed. Received malformed response."))
        return Tr::tr("Autodiscovery failed. Received malformed response.");
    if (error == QLatin1String("Autodiscovery failed. Unknown error when requesting .well-known."))
        return Tr::tr("Autodiscovery failed. Unknown error when requesting .well-known.");
    if (error ==
        QLatin1String("The required endpoints were not found. Possibly not a Matrix server."))
        return Tr::tr("The required endpoints were not found. Possibly not a Matrix server.");
    if (error ==
        QLatin1String(
          "Server does not require any authentication for registration. This is unexpected."))
        return Tr::tr(
          "Server does not require any authentication for registration. This is unexpected.");
    if (error ==
        QLatin1String("Server returned no registration flows. Registration may be disabled."))
        return Tr::tr("Server returned no registration flows. Registration may be disabled.");
    if (error == QLatin1String("Registration is disabled on this server."))
        return Tr::tr("Registration is disabled on this server.");
    if (error == QLatin1String("Registration token cannot be empty"))
        return Tr::tr("Registration token cannot be empty");
    if (error == QLatin1String("OAuth callback query cannot be empty"))
        return Tr::tr("OAuth callback query cannot be empty");

    // ── Dynamic strings with translatable prefix ──
    // Pattern: "Prefix: SDK-error-detail"

    QString result;

    result = translateErrorPrefix(
      error, QT_TRANSLATE_NOOP("StateEventText", "Failed to contact the homeserver:"));
    if (!result.isEmpty())
        return result;

    result = translateErrorPrefix(
      error, QT_TRANSLATE_NOOP("StateEventText", "Failed to discover Matrix login flows:"));
    if (!result.isEmpty())
        return result;

    result =
      translateErrorPrefix(error, QT_TRANSLATE_NOOP("StateEventText", "Registration failed:"));
    if (!result.isEmpty())
        return result;

    result = translateErrorPrefix(
      error,
      QT_TRANSLATE_NOOP("StateEventText", "Autodiscovery failed while requesting .well-known:"));
    if (!result.isEmpty())
        return result;

    // Unrecognised error -- return as-is.
    return error;
}

} // namespace StateEventText
