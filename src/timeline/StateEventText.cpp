// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StateEventText.h"

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include <QCoreApplication>

// Short alias: all tr() calls go through this context so that lupdate
// groups them under "StateEventText" in the .ts files.
#define TR(text) QCoreApplication::translate("StateEventText", text)

namespace StateEventText {

// ── Membership changes ──────────────────────────────────────────────

static QString
translateMembershipChange(const MatrixTimelineItem &item)
{
    const auto &user     = item.stateEventTargetUser;
    const auto &sender   = item.senderDisplayName;
    const auto &reason   = item.stateEventReason;
    const bool hasSender = item.stateEventHasSender;
    const bool hasReason = !reason.isEmpty();
    const auto &kind     = item.membershipChangeKind;

    // Each combination of (hasSender, hasReason) is a distinct full sentence
    // so that translators can reorder all parts freely.

    if (kind == QStringLiteral("joined"))
        return TR("%1 joined the room").arg(user);

    if (kind == QStringLiteral("left"))
        return TR("%1 left the room").arg(user);

    if (kind == QStringLiteral("banned")) {
        if (hasSender && hasReason)
            return TR("%1 was banned by %2: %3").arg(user, sender, reason);
        if (hasSender)
            return TR("%1 was banned by %2").arg(user, sender);
        if (hasReason)
            return TR("%1 was banned: %2").arg(user, reason);
        return TR("%1 was banned").arg(user);
    }

    if (kind == QStringLiteral("unbanned")) {
        if (hasSender)
            return TR("%1 was unbanned by %2").arg(user, sender);
        return TR("%1 was unbanned").arg(user);
    }

    if (kind == QStringLiteral("kicked")) {
        if (hasSender && hasReason)
            return TR("%1 was kicked by %2: %3").arg(user, sender, reason);
        if (hasSender)
            return TR("%1 was kicked by %2").arg(user, sender);
        if (hasReason)
            return TR("%1 was kicked: %2").arg(user, reason);
        return TR("%1 was kicked").arg(user);
    }

    if (kind == QStringLiteral("invited")) {
        if (hasSender)
            return TR("%1 was invited by %2").arg(user, sender);
        return TR("%1 was invited").arg(user);
    }

    if (kind == QStringLiteral("kicked_and_banned")) {
        if (hasSender && hasReason)
            return TR("%1 was kicked and banned by %2: %3").arg(user, sender, reason);
        if (hasSender)
            return TR("%1 was kicked and banned by %2").arg(user, sender);
        if (hasReason)
            return TR("%1 was kicked and banned: %2").arg(user, reason);
        return TR("%1 was kicked and banned").arg(user);
    }

    if (kind == QStringLiteral("invitation_accepted"))
        return TR("%1 accepted the invite").arg(user);

    if (kind == QStringLiteral("invitation_rejected"))
        return TR("%1 rejected the invite").arg(user);

    if (kind == QStringLiteral("invitation_revoked")) {
        if (hasSender)
            return TR("%1's invite was revoked by %2").arg(user, sender);
        return TR("%1's invite was revoked").arg(user);
    }

    if (kind == QStringLiteral("knocked"))
        return TR("%1 requested to join").arg(user);

    if (kind == QStringLiteral("knock_accepted")) {
        if (hasSender)
            return TR("%1's knock was accepted by %2").arg(user, sender);
        return TR("%1's knock was accepted").arg(user);
    }

    if (kind == QStringLiteral("knock_retracted"))
        return TR("%1 withdrew the join request").arg(user);

    if (kind == QStringLiteral("knock_denied")) {
        if (hasSender)
            return TR("%1's join request was denied by %2").arg(user, sender);
        return TR("%1's join request was denied").arg(user);
    }

    if (kind == QStringLiteral("redacted"))
        return TR("Redacted membership event for %1").arg(user);

    // none / error / not_implemented
    return TR("Membership updated for %1").arg(user);
}

// ── Profile changes ─────────────────────────────────────────────────

static QString
translateProfileChange(const MatrixTimelineItem &item)
{
    const auto &user   = item.stateEventTargetUser;
    const auto &detail = item.stateEventDetail;

    // membershipChangeKind is reused to carry the profile-change sub-kind.
    const auto &subKind = item.membershipChangeKind;

    if (subKind == QStringLiteral("displayname") && !detail.isEmpty())
        return TR("%1 is now known as %2").arg(user, detail);

    if (subKind == QStringLiteral("displayname_removed"))
        return TR("%1 removed their display name").arg(user);

    if (subKind == QStringLiteral("avatar"))
        return TR("%1 changed their avatar").arg(user);

    return TR("%1 updated their profile").arg(user);
}

// ── Room state changes ──────────────────────────────────────────────

static QString
translateRoomName(const MatrixTimelineItem &item)
{
    const auto &sender = item.senderDisplayName;
    const auto &detail = item.stateEventDetail;

    if (!detail.isEmpty())
        return TR("%1 changed the room name to: %2").arg(sender, detail);
    return TR("%1 removed the room name").arg(sender);
}

static QString
translateRoomTopic(const MatrixTimelineItem &item)
{
    const auto &sender = item.senderDisplayName;
    const auto &detail = item.stateEventDetail;

    if (!detail.isEmpty())
        return TR("%1 changed the topic to: %2").arg(sender, detail);
    return TR("%1 removed the topic").arg(sender);
}

static QString
translateJoinRules(const MatrixTimelineItem &item)
{
    const auto &sender = item.senderDisplayName;
    const auto &detail = item.stateEventDetail;

    if (detail == QStringLiteral("invite"))
        return TR("%1 changed the room access rules to invite-only").arg(sender);
    if (detail == QStringLiteral("knock"))
        return TR("%1 changed the room access rules to knock-to-join").arg(sender);
    if (detail == QStringLiteral("public"))
        return TR("%1 changed the room access rules to public").arg(sender);
    if (detail == QStringLiteral("private"))
        return TR("%1 changed the room access rules to private").arg(sender);
    if (detail == QStringLiteral("restricted"))
        return TR("%1 changed the room access rules to restricted").arg(sender);
    if (detail == QStringLiteral("knock_restricted"))
        return TR("%1 changed the room access rules to knock (restricted)").arg(sender);

    return TR("%1 changed the room access rules").arg(sender);
}

static QString
translateHistoryVisibility(const MatrixTimelineItem &item)
{
    const auto &sender = item.senderDisplayName;
    const auto &detail = item.stateEventDetail;

    if (detail == QStringLiteral("invited"))
        return TR("%1 changed the room history visibility to visible since invite").arg(sender);
    if (detail == QStringLiteral("joined"))
        return TR("%1 changed the room history visibility to visible since join").arg(sender);
    if (detail == QStringLiteral("shared"))
        return TR("%1 changed the room history visibility to shared").arg(sender);
    if (detail == QStringLiteral("world_readable"))
        return TR("%1 changed the room history visibility to world-readable").arg(sender);

    return TR("%1 changed the room history visibility").arg(sender);
}

static QString
translateGuestAccess(const MatrixTimelineItem &item)
{
    const auto &sender = item.senderDisplayName;
    const auto &detail = item.stateEventDetail;

    if (detail == QStringLiteral("can_join"))
        return TR("%1 changed the room guest access to allowed").arg(sender);
    if (detail == QStringLiteral("forbidden"))
        return TR("%1 changed the room guest access to forbidden").arg(sender);

    return TR("%1 changed the room guest access").arg(sender);
}

static QString
translateOtherState(const MatrixTimelineItem &item)
{
    const auto &sender    = item.senderDisplayName;
    const auto &eventType = item.matrixEventType;

    if (eventType == QStringLiteral("m.room.name"))
        return translateRoomName(item);
    if (eventType == QStringLiteral("m.room.topic"))
        return translateRoomTopic(item);
    if (eventType == QStringLiteral("m.room.avatar"))
        return TR("%1 changed the room avatar").arg(sender);
    if (eventType == QStringLiteral("m.room.encryption"))
        return TR("%1 enabled end-to-end encryption").arg(sender);
    if (eventType == QStringLiteral("m.room.pinned_events"))
        return TR("%1 changed the pinned messages").arg(sender);
    if (eventType == QStringLiteral("m.room.power_levels"))
        return TR("%1 changed the room permissions").arg(sender);
    if (eventType == QStringLiteral("m.room.join_rules"))
        return translateJoinRules(item);
    if (eventType == QStringLiteral("m.room.history_visibility"))
        return translateHistoryVisibility(item);
    if (eventType == QStringLiteral("m.room.guest_access"))
        return translateGuestAccess(item);
    if (eventType == QStringLiteral("m.room.canonical_alias"))
        return TR("%1 changed the addresses for this room").arg(sender);
    if (eventType == QStringLiteral("m.room.tombstone"))
        return TR("%1 replaced this room").arg(sender);
    if (eventType == QStringLiteral("m.room.server_acl"))
        return TR("%1 changed which servers are allowed in this room").arg(sender);
    if (eventType == QStringLiteral("m.room.create"))
        return TR("%1 created and configured the room").arg(sender);
    if (eventType == QStringLiteral("m.space.parent"))
        return TR("%1 changed the parent communities for this room").arg(sender);
    if (eventType == QStringLiteral("m.space.child"))
        return TR("%1 changed a child room of this space").arg(sender);
    if (eventType == QStringLiteral("m.policy.rule.room") ||
        eventType == QStringLiteral("m.policy.rule.user") ||
        eventType == QStringLiteral("m.policy.rule.server"))
        return TR("%1 updated a moderation policy rule").arg(sender);

    return TR("%1 changed unknown state event %2").arg(sender, eventType);
}

// ── Event type labels ───────────────────────────────────────────────

QString
eventTypeLabel(const QString &itemKind, const QString & /*matrixEventType*/)
{
    if (itemKind == QStringLiteral("redacted"))
        return TR("Deleted message");
    if (itemKind == QStringLiteral("unable_to_decrypt"))
        return TR("[Unable to decrypt message]");
    if (itemKind == QStringLiteral("failed_to_parse_message_like"))
        return TR("[Unreadable message event]");
    if (itemKind == QStringLiteral("failed_to_parse_state"))
        return TR("[Unreadable state event]");
    if (itemKind == QStringLiteral("call_invite"))
        return TR("[Call invite]");
    if (itemKind == QStringLiteral("rtc_notification"))
        return TR("[RTC notification]");
    if (itemKind == QStringLiteral("poll"))
        return TR("[Poll]");
    if (itemKind == QStringLiteral("sticker"))
        return TR("[Sticker]");
    if (itemKind == QStringLiteral("reaction"))
        return TR("Reactions updated");
    if (itemKind == QStringLiteral("other_message"))
        return TR("[Unsupported message event]");
    if (itemKind == QStringLiteral("unknown_message"))
        return TR("[Unsupported message event]");

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
        return TR("Room state changed by %1").arg(item.senderDisplayName);

    return {};
}

// ── Notification body translation ───────────────────────────────────

QString
translateNotificationBody(const MatrixNotificationItem &notification)
{
    const auto &kind = notification.notificationKind;

    // Invite notifications have no body from Rust -- provide translated text.
    if (kind == QStringLiteral("invite"))
        return TR("Invited you to join this room");

    // For event-type kinds that have translatable labels, use eventTypeLabel().
    // This covers: redacted, unable_to_decrypt, poll, call_invite, sticker,
    // reaction, other_message, unknown_message, etc.
    const auto label = eventTypeLabel(kind, {});
    if (!label.isEmpty())
        return label;

    // For membership/state events shown in notifications, provide a generic label.
    if (kind == QStringLiteral("membership_change"))
        return TR("[Membership change]");
    if (kind == QStringLiteral("other_state"))
        return TR("[Room state changed]");

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
        return TR("[Membership change]");
    if (kind == QStringLiteral("profile_change"))
        return TR("[Profile updated]");
    if (kind == QStringLiteral("other_state"))
        return TR("[Room state changed]");

    // Content-bearing kinds -- use original body as-is
    return body;
}

// ── Auth / registration error translation ───────────────────────────
//
// Known constant error strings from Rust auth.rs and registration.rs.
// When changing error strings in those files, update the mappings here.

namespace {

// Try to split "Prefix: detail" and translate just the prefix.
// Returns empty string if the error does not start with the prefix.
QString
translateErrorPrefix(const QString &error, const QLatin1String &prefix, const char *translated)
{
    if (!error.startsWith(prefix))
        return {};

    const auto detail = error.mid(prefix.size()).trimmed();
    if (detail.isEmpty())
        return TR(translated);

    return QStringLiteral("%1 %2").arg(TR(translated), detail);
}

} // anonymous namespace

QString
translateAuthError(const QString &error)
{
    // ── Constant strings (exact match) ──

    if (error ==
        QLatin1String("Received malformed response. Make sure the homeserver domain is valid."))
        return TR("Received malformed response. Make sure the homeserver domain is valid.");
    if (error == QLatin1String("Autodiscovery failed. Received malformed response."))
        return TR("Autodiscovery failed. Received malformed response.");
    if (error == QLatin1String("Autodiscovery failed. Unknown error when requesting .well-known."))
        return TR("Autodiscovery failed. Unknown error when requesting .well-known.");
    if (error ==
        QLatin1String("The required endpoints were not found. Possibly not a Matrix server."))
        return TR("The required endpoints were not found. Possibly not a Matrix server.");
    if (error ==
        QLatin1String(
          "Server does not require any authentication for registration. This is unexpected."))
        return TR(
          "Server does not require any authentication for registration. This is unexpected.");
    if (error ==
        QLatin1String("Server returned no registration flows. Registration may be disabled."))
        return TR("Server returned no registration flows. Registration may be disabled.");
    if (error == QLatin1String("Registration is disabled on this server."))
        return TR("Registration is disabled on this server.");
    if (error == QLatin1String("Registration token cannot be empty"))
        return TR("Registration token cannot be empty");
    if (error == QLatin1String("OAuth callback query cannot be empty"))
        return TR("OAuth callback query cannot be empty");

    // ── Dynamic strings with translatable prefix ──
    // Pattern: "Prefix: SDK-error-detail"

    QString result;

    result = translateErrorPrefix(error,
                                  QLatin1String("Failed to contact the homeserver:"),
                                  "Failed to contact the homeserver:");
    if (!result.isEmpty())
        return result;

    result = translateErrorPrefix(error,
                                  QLatin1String("Failed to discover Matrix login flows:"),
                                  "Failed to discover Matrix login flows:");
    if (!result.isEmpty())
        return result;

    result =
      translateErrorPrefix(error, QLatin1String("Registration failed:"), "Registration failed:");
    if (!result.isEmpty())
        return result;

    result =
      translateErrorPrefix(error,
                           QLatin1String("Autodiscovery failed while requesting .well-known:"),
                           "Autodiscovery failed while requesting .well-known:");
    if (!result.isEmpty())
        return result;

    // Unrecognised error -- return as-is.
    return error;
}

} // namespace StateEventText
