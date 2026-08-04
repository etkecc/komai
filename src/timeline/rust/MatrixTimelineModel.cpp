// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/rust/MatrixTimelineModel.h"

#include "encryption/CryptoTrust.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/StateEventText.h"
#include "timeline/TimelineEventTypes.h"
#include "timeline/TimelineViewManager.h"
#include "ui/KomaiGlobalObject.h"
#include "utils/MediaIcons.h"
#include "utils/Utils.h"

#include <QByteArray>
#include <QDateTime>
#include <QGuiApplication>
#include <QHash>
#include <QLocale>
#include <QPalette>
#include <QTextDocument>
#include <QUrl>
#include <algorithm>

#include "komai-rust-cxxbridge/ffi.h"

namespace komai {

namespace {
std::optional<int>
configuredInitialVisibleWindow()
{
    const auto value = qgetenv("KOMAI_PERF_MATRIX_TIMELINE_INITIAL_WINDOW").trimmed();
    if (value.isEmpty())
        return std::nullopt;

    bool ok          = false;
    const auto count = value.toInt(&ok);
    if (!ok || count <= 0)
        return std::nullopt;

    return count;
}

bool
isStateLikeKind(const QString &kind)
{
    return kind == QStringLiteral("membership_change") ||
           kind == QStringLiteral("profile_change") || kind == QStringLiteral("other_state") ||
           kind == QStringLiteral("failed_to_parse_state");
}

int
dayKeyFromTimestamp(uint64_t timestampMs)
{
    if (timestampMs == 0)
        return 0;
    auto date = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestampMs)).date();
    return date.year() * 10000 + date.month() * 100 + date.day();
}

int
deliveryStateToEventState(const QString &state)
{
    if (state == QStringLiteral("pending"))
        return qml_mtx_events::Pending;
    if (state == QStringLiteral("sent"))
        return qml_mtx_events::Sent;
    if (state == QStringLiteral("read"))
        return qml_mtx_events::Read;
    if (state == QStringLiteral("failed"))
        return qml_mtx_events::Failed;
    return qml_mtx_events::Empty;
}

QString
defaultMembershipStateEventIcon()
{
    return QStringLiteral(":/icons/icons/ui/state-event.svg");
}

QString
stateEventIconForMembershipChangeKind(const QString &membershipChangeKind)
{
    const auto kind = membershipChangeKind.trimmed().toLower();

    if (kind == QStringLiteral("joined"))
        return QStringLiteral(":/icons/icons/ui/state-member-join.svg");

    // Member gained or restriction lifted
    if (kind == QStringLiteral("invited") || kind == QStringLiteral("invitation_accepted") ||
        kind == QStringLiteral("knock_accepted") || kind == QStringLiteral("unbanned"))
        return QStringLiteral(":/icons/icons/ui/state-member-change.svg");

    // Departure or withdrawn participation, voluntary or forced
    if (kind == QStringLiteral("left") || kind == QStringLiteral("kicked") ||
        kind == QStringLiteral("invitation_rejected") ||
        kind == QStringLiteral("invitation_revoked") || kind == QStringLiteral("knock_retracted"))
        return QStringLiteral(":/icons/icons/ui/state-member-leave.svg");

    // Blocked entry
    if (kind == QStringLiteral("banned") || kind == QStringLiteral("kicked_and_banned") ||
        kind == QStringLiteral("knock_denied"))
        return QStringLiteral(":/icons/icons/ui/presence-blocked.svg");

    return defaultMembershipStateEventIcon();
}

QString
stateEventIconForKind(const QString &kind)
{
    if (kind == QStringLiteral("membership_change"))
        return defaultMembershipStateEventIcon();
    if (kind == QStringLiteral("profile_change"))
        return QStringLiteral(":/icons/icons/ui/state-member-display-name.svg");
    return QStringLiteral(":/icons/icons/ui/state-event.svg");
}

QString
stateEventIconColorCategoryForMembershipChangeKind(const QString &membershipChangeKind)
{
    const auto kind = membershipChangeKind.trimmed().toLower();

    // Positive: gaining a member or lifting a restriction
    if (kind == QStringLiteral("joined") || kind == QStringLiteral("invited") ||
        kind == QStringLiteral("invitation_accepted") || kind == QStringLiteral("knock_accepted") ||
        kind == QStringLiteral("unbanned"))
        return QStringLiteral("positive");

    // Negative: punitive moderation actions
    if (kind == QStringLiteral("banned") || kind == QStringLiteral("kicked") ||
        kind == QStringLiteral("kicked_and_banned") || kind == QStringLiteral("knock_denied"))
        return QStringLiteral("negative");

    // Cautious: departures and declined participation
    if (kind == QStringLiteral("left") || kind == QStringLiteral("invitation_rejected") ||
        kind == QStringLiteral("invitation_revoked") || kind == QStringLiteral("knock_retracted") ||
        kind == QStringLiteral("redacted"))
        return QStringLiteral("cautious");

    return QStringLiteral("neutral");
}

QString
stateEventIconColorCategoryForItem(const MatrixTimelineItem &item)
{
    if (item.itemKind == QStringLiteral("membership_change"))
        return stateEventIconColorCategoryForMembershipChangeKind(item.membershipChangeKind);

    // Positive: security improvement or room genesis
    if (item.matrixEventType == QStringLiteral("m.room.encryption") ||
        item.matrixEventType == QStringLiteral("m.room.create"))
        return QStringLiteral("positive");

    // Negative: room replaced/killed
    if (item.matrixEventType == QStringLiteral("m.room.tombstone"))
        return QStringLiteral("negative");

    // Cautious: server access restrictions (defensive but noteworthy)
    if (item.matrixEventType == QStringLiteral("m.room.server_acl"))
        return QStringLiteral("cautious");

    // Power levels: positive if all promotions, negative if all demotions,
    // cautious if mixed, neutral if no user changes.
    if (item.matrixEventType == QStringLiteral("m.room.power_levels") &&
        !item.powerLevelChanges.empty()) {
        bool hasPromotion = false;
        bool hasDemotion  = false;
        for (const auto &change : item.powerLevelChanges) {
            if (change.newLevel > change.oldLevel)
                hasPromotion = true;
            else if (change.newLevel < change.oldLevel)
                hasDemotion = true;
        }
        if (hasPromotion && !hasDemotion)
            return QStringLiteral("positive");
        if (hasDemotion && !hasPromotion)
            return QStringLiteral("negative");
        if (hasPromotion && hasDemotion)
            return QStringLiteral("cautious");
    }

    return QStringLiteral("neutral");
}

QString
stateEventIconForItem(const MatrixTimelineItem &item)
{
    if (item.itemKind == QStringLiteral("membership_change"))
        return stateEventIconForMembershipChangeKind(item.membershipChangeKind);

    if (item.matrixEventType == QStringLiteral("m.room.pinned_events"))
        return QStringLiteral(":/icons/icons/ui/pin.svg");
    if (item.matrixEventType == QStringLiteral("m.room.server_acl"))
        return QStringLiteral(":/icons/icons/ui/stop.svg");
    if (item.matrixEventType == QStringLiteral("m.room.power_levels"))
        return QStringLiteral(":/icons/icons/ui/arrow-sort.svg");
    if (item.matrixEventType == QStringLiteral("m.room.create"))
        return QStringLiteral(":/icons/icons/ui/hammer.svg");

    return stateEventIconForKind(item.itemKind);
}

QString
formatBodyHtml(const QString &body,
               const QString &formattedBody                                  = {},
               const ::rust::Vec<::komai::rust::HtmlPillAvatar> &pillAvatars = {})
{
    if (body.isEmpty() && formattedBody.isEmpty())
        return {};

    const auto bodyStd          = body.toStdString();
    const auto formattedBodyStd = formattedBody.toStdString();

    const auto settings        = UserSettings::instance();
    const bool syntaxHighlight = settings && settings->timelineFormattedCodeSyntaxHighlighting();
    const bool isDark          = QGuiApplication::palette().color(QPalette::Base).lightness() < 128;
    const int pillAvatarSize   = Komai::iconLogicalSize();

    auto html = QString::fromStdString(std::string(
      komai::rust::format_body_html(::rust::Str(bodyStd.data(), bodyStd.size()),
                                    ::rust::Str(formattedBodyStd.data(), formattedBodyStd.size()),
                                    pillAvatars,
                                    static_cast<uint32_t>(pillAvatarSize),
                                    isDark,
                                    syntaxHighlight)));

    return utils::replaceEmoji(html);
}

QString
htmlToPlainText(const QString &html)
{
    if (html.isEmpty())
        return {};

    QTextDocument document;
    document.setHtml(html);
    auto text = document.toPlainText();
    // `<img>` elements (pill avatars for matrix.to mentions) become U+FFFC in
    // toPlainText() output. Drop them so copy reads cleanly. Emoji uses span
    // elements, not img, so it survives unaffected.
    text.remove(QChar::ObjectReplacementCharacter);
    return text;
}

QString
emoteSenderPrefix(const MatrixTimelineItem &item)
{
    const auto sender = !item.senderDisplayName.isEmpty() ? item.senderDisplayName : item.senderId;
    return sender.isEmpty() ? QString() : sender + QStringLiteral(" ");
}

QString
originalCopyTextForItem(const MatrixTimelineItem &item)
{
    // Membership/profile state events have an empty body; without this they'd be
    // dropped from a multi-selection copy (empty strings are filtered upstream).
    if (item.cachedIsStateEvent) {
        const auto translated = StateEventText::translate(item);
        if (!translated.isEmpty())
            return translated;
        const auto fallback = htmlToPlainText(item.cachedFormattedStateEvent);
        if (!fallback.isEmpty())
            return fallback;
        return item.body;
    }

    // Match the displayed text: TimelineEvent.qml prepends the sender display name to emote bodies.
    if (item.itemKind == QStringLiteral("emote") && !item.body.isEmpty())
        return emoteSenderPrefix(item) + item.body;

    return item.body;
}

QString
plainCopyTextForItem(const MatrixTimelineItem &item)
{
    const auto html =
      item.cachedIsStateEvent ? item.cachedFormattedStateEvent : item.cachedFormattedBody;
    auto text = htmlToPlainText(html);
    if (!text.isEmpty()) {
        if (!item.cachedIsStateEvent && item.itemKind == QStringLiteral("emote"))
            text = emoteSenderPrefix(item) + text;
        return text;
    }

    return originalCopyTextForItem(item);
}

QString
copyHeaderForItem(const MatrixTimelineItem &item)
{
    QString senderText;
    if (!item.senderId.isEmpty()) {
        senderText = item.senderId;
        if (!item.senderDisplayName.isEmpty() && item.senderDisplayName != item.senderId)
            senderText += QStringLiteral(" (") + item.senderDisplayName + QStringLiteral(")");
    } else if (!item.senderDisplayName.isEmpty()) {
        senderText = item.senderDisplayName;
    }

    QString timestampText;
    if (item.timestamp != 0) {
        const auto dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.timestamp));
        timestampText = QLocale::system().toString(dt, QLocale::ShortFormat);
    }

    QString header;
    if (!timestampText.isEmpty())
        header += QStringLiteral("[") + timestampText + QStringLiteral("] ");
    if (!senderText.isEmpty())
        header += senderText + QStringLiteral(": ");
    return header;
}

QString
stableTimelineItemKey(const MatrixTimelineItem &item)
{
    const auto eventId = item.eventId.trimmed();
    if (!eventId.isEmpty())
        return eventId;
    return item.itemId.trimmed();
}

QString
effectiveReplyPreviewBody(const MatrixTimelineItem &item)
{
    if (!item.replyBody.isEmpty() || !item.replyFormattedBody.isEmpty())
        return item.replyBody;

    return QStringLiteral("[Original message unavailable]");
}

QString
effectiveReplyPreviewDisplayName(const MatrixTimelineItem &item)
{
    if (!item.replySenderDisplayName.trimmed().isEmpty())
        return item.replySenderDisplayName.trimmed();
    if (!item.replySenderId.trimmed().isEmpty())
        return item.replySenderId.trimmed();

    return QStringLiteral("Unknown sender");
}

// Translate matrix-sdk-ui's shield tags into Komai's 4-bucket `crypto::Trust`.
// Keep the mapping coarse: the QML indicator only distinguishes verified /
// TOFU / unverified / message-unverified, so grey shields collapse into
// MessageUnverified (or TOFU when the grey code is `authenticity_not_guaranteed`)
// and every red shield becomes Unverified.
int
trustlevelFromShield(const QString &shieldColor, const QString &shieldCode)
{
    if (shieldColor.isEmpty())
        return static_cast<int>(crypto::Trust::Verified);
    if (shieldColor == QLatin1String("grey")) {
        if (shieldCode == QLatin1String("authenticity_not_guaranteed"))
            return static_cast<int>(crypto::Trust::TOFU);
        return static_cast<int>(crypto::Trust::MessageUnverified);
    }
    return static_cast<int>(crypto::Trust::Unverified);
}

// Map the snake_case `shield_code` string (from the Rust FFI, originally
// matrix-sdk-common's `ShieldStateCode`) onto the richer `crypto::MessageShield`
// enum. Empty/unknown → ShieldNone; unrecognised codes fall through to
// ShieldAuthenticityNotGuaranteed so the UI shows a generic grey shield
// instead of silently claiming the message is clean.
int
messageShieldFromCode(const QString &shieldColor, const QString &shieldCode)
{
    if (shieldColor.isEmpty())
        return static_cast<int>(crypto::MessageShield::ShieldNone);
    if (shieldCode == QLatin1String("authenticity_not_guaranteed"))
        return static_cast<int>(crypto::MessageShield::ShieldAuthenticityNotGuaranteed);
    if (shieldCode == QLatin1String("unknown_device"))
        return static_cast<int>(crypto::MessageShield::ShieldUnknownDevice);
    if (shieldCode == QLatin1String("unsigned_device"))
        return static_cast<int>(crypto::MessageShield::ShieldUnsignedDevice);
    if (shieldCode == QLatin1String("sent_in_clear"))
        return static_cast<int>(crypto::MessageShield::ShieldSentInClear);
    if (shieldCode == QLatin1String("unverified_identity"))
        return static_cast<int>(crypto::MessageShield::ShieldUnverifiedIdentity);
    if (shieldCode == QLatin1String("verification_violation"))
        return static_cast<int>(crypto::MessageShield::ShieldVerificationViolation);
    if (shieldCode == QLatin1String("mismatched_sender"))
        return static_cast<int>(crypto::MessageShield::ShieldMismatchedSender);
    return static_cast<int>(crypto::MessageShield::ShieldAuthenticityNotGuaranteed);
}

void
computeDerivedFields(MatrixTimelineItem &item,
                     const QString &roomId,
                     const ::rust::Vec<::komai::rust::HtmlPillAvatar> &pillAvatars = {})
{
    const bool isState = isStateLikeKind(item.itemKind);
    item.cachedType = qml_mtx_events::matrixTimelineEventType(item.itemKind, item.matrixEventType);
    // matrix-sdk-ui's default timeline event filter rejects every call
    // lifecycle event except `m.call.invite`, so after sync these never
    // appear at all. The send-queue's local echo bypasses that filter
    // though, so an outgoing hangup/answer/reject would render a "ghost"
    // entry that disappears on restart and never receives a delivery
    // upgrade (no remote peer can ever read it). Hide these unconditionally
    // so the local echo also vanishes.
    const bool isAlwaysHiddenCallLifecycle = item.cachedType == qml_mtx_events::CallAnswer ||
                                             item.cachedType == qml_mtx_events::CallHangUp ||
                                             item.cachedType == qml_mtx_events::CallReject ||
                                             item.cachedType == qml_mtx_events::CallSelectAnswer ||
                                             item.cachedType == qml_mtx_events::CallNegotiate ||
                                             item.cachedType == qml_mtx_events::CallCandidates;
    // MatrixRTC (Element Call) membership state events churn constantly as
    // participants join, leave and refresh their per-device session state.
    // Komai has no renderer for them, so they would otherwise show up as
    // generic "changed unknown state event" notices; hide them like every
    // other client does (Element Web/Cinny render no row for them either).
    const bool isRtcMembership =
      item.matrixEventType == QStringLiteral("org.matrix.msc3401.call.member");
    item.cachedIsHiddenEvent =
      isAlwaysHiddenCallLifecycle || isRtcMembership ||
      (UserSettings::instance() &&
       UserSettings::instance()->isTimelineEventHiddenInRoom(item.cachedType, roomId));
    item.cachedEmojiOnlyCount = item.cachedType == qml_mtx_events::TextMessage
                                  ? utils::emojiOnlyCodepointCount(item.body)
                                  : 0;
    item.cachedDay            = dayKeyFromTimestamp(item.timestamp);
    item.cachedStatus         = deliveryStateToEventState(item.deliveryState);
    item.cachedIsStateEvent   = isState;
    item.cachedIsEncrypted    = item.isEncryptedEvent;
    item.cachedIsEditable     = item.isOwn && (item.itemKind == QStringLiteral("message") ||
                                           item.itemKind == QStringLiteral("notice") ||
                                           item.itemKind == QStringLiteral("emote"));
    item.cachedProportionalH  = (item.mediaWidth > 0 && item.mediaHeight > 0)
                                  ? static_cast<double>(item.mediaHeight) / item.mediaWidth
                                  : 0.0;
    item.cachedFormattedBody =
      isState ? QString() : formatBodyHtml(item.body, item.formattedBody, pillAvatars);

    if (isState) {
        const auto translated = StateEventText::translate(item);
        item.cachedFormattedStateEvent =
          formatBodyHtml(!translated.isEmpty() ? translated : item.body, {});
    } else {
        item.cachedFormattedStateEvent = {};
    }
    item.cachedStateEventIcon = isState ? stateEventIconForItem(item) : QString();
    item.cachedStateEventIconColorCategory =
      isState ? stateEventIconColorCategoryForItem(item) : QString();
    item.cachedFilesize =
      item.mediaSizeBytes > 0 ? utils::humanReadableFileSize(item.mediaSizeBytes) : QString();
    item.cachedFilename =
      item.fileName.isEmpty() ? (item.body.isEmpty() ? QString() : item.body) : item.fileName;
    item.cachedFileTypeIcon = utils::fileTypeIconSource(item.mimeType);
}

} // namespace

MatrixTimelineModel::MatrixTimelineModel(QObject *parent)
  : EventDataSource(parent)
{
    if (const auto settings = UserSettings::instance()) {
        connect(settings.get(),
                &UserSettings::hiddenTimelineEventTypesChanged,
                this,
                &MatrixTimelineModel::refreshDerivedFields);
        // The pill-avatar URLs in cachedFormattedBody bake in the radius and
        // default-avatar style at HTML-generation time, so a setting flip
        // doesn't reach the litehtml renderer until we regenerate the body
        // HTML. Avatar.qml in the timeline body re-evaluates its bindings
        // automatically; pills are static HTML and need this nudge.
        connect(settings.get(),
                &UserSettings::uiAvatarsCircularChanged,
                this,
                &MatrixTimelineModel::refreshDerivedFields);
        connect(settings.get(),
                &UserSettings::uiAvatarsDefaultAvatarStyleChanged,
                this,
                &MatrixTimelineModel::refreshDerivedFields);
        // Syntax highlighting bakes token colors into cachedFormattedBody as
        // inline styles, chosen from the palette in effect at generation time.
        // Without these, already-loaded messages keep the previous theme's
        // colors until their rows happen to be rebuilt. uiThemeSlugChanged also
        // covers Auto mode's light/dark flips, which route through the slug.
        connect(settings.get(),
                &UserSettings::uiThemeSlugChanged,
                this,
                &MatrixTimelineModel::refreshDerivedFields);
        connect(settings.get(),
                &UserSettings::timelineFormattedCodeSyntaxHighlightingChanged,
                this,
                &MatrixTimelineModel::refreshDerivedFields);
    }
}

void
MatrixTimelineModel::setRoomId(const QString &roomId)
{
    const auto normalizedRoomId = roomId.trimmed();
    if (roomId_ == normalizedRoomId)
        return;

    roomId_ = normalizedRoomId;
    refreshDerivedFields();
}

void
MatrixTimelineModel::setPaginationInProgress(bool inProgress)
{
    if (paginationInProgress_ == inProgress)
        return;

    paginationInProgress_ = inProgress;
    emit paginationInProgressChanged();
}

int
MatrixTimelineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return items_.size();
}

QVariant
MatrixTimelineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
        return {};

    const auto &item = items_.at(index.row());

    // clang-format off
    switch (role) {
    // --- TimelineModel-compatible roles (most use pre-computed cached fields) ---
    case Type:               return item.cachedType;
    case TypeString:         return item.itemKind;
    case IsOnlyEmoji:        return item.cachedEmojiOnlyCount;
    case Body:               return item.body;
    case FormattedBody:      return item.cachedFormattedBody;
    case HasFormattedBody:   return !item.cachedIsStateEvent && !item.formattedBody.isEmpty();
    case FormattedStateEvent:return item.cachedFormattedStateEvent;
    case StateEventIconSource:return item.cachedStateEventIcon;
    case StateEventIconColorCategory:return item.cachedStateEventIconColorCategory;
    case IsSender:           return item.isOwn;
    case UserId:             return item.senderId;
    case UserName:           return item.senderDisplayName;
    case UserPowerlevel:     return 0;
    case Day:                return item.cachedDay;
    case Timestamp:          return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.timestamp));
    case Url:                return item.mediaUrl;
    case ThumbnailUrl:       return item.thumbnailUrl;
    case Duration:           return static_cast<int>(item.mediaDurationMs);
    case Blurhash:           return item.blurhash;
    case Filename:           return item.cachedFilename;
    case Filesize:           return item.cachedFilesize;
    case FilesizeBytes:      return static_cast<int>(item.mediaSizeBytes);
    case MimeType:           return item.mimeType;
    case OriginalHeight:     return static_cast<int>(item.mediaHeight);
    case OriginalWidth:      return static_cast<int>(item.mediaWidth);
    case ProportionalHeight: return item.cachedProportionalH;
    case EventId:            return item.eventId;
    case IsLatestCallNotification:
        return !item.eventId.isEmpty() && item.eventId == latestRtcNotificationEventId_;
    case Status:             return item.cachedStatus;
    case IsEdited:           return item.isEdited;
    case IsEditable:         return item.cachedIsEditable;
    case IsEncrypted:        return item.cachedIsEncrypted;
    case IsStateEvent:       return item.cachedIsStateEvent;
    case Trustlevel:         return trustlevelFromShield(item.shieldColor, item.shieldCode);
    case Notificationlevel:  return static_cast<int>(qml_mtx_events::Nothing);
    case UtdCause:           return item.utdCause;
    case ReplyTo:            return item.cachedIsStateEvent ? QString() : item.replyEventId;
    case ThreadId:           return item.threadId;
    case Reactions:          return item.reactions;
    case Room:               return false;
    case RoomId:             return roomId_;
    case CallType:           return QString();
    case Dump:               return QVariant();
    case RelatedEventCacheBuster: return 0;
    case IsHiddenEvent:      return item.cachedIsHiddenEvent;
    case FileTypeIconSource: return item.cachedFileTypeIcon;

    // --- Extra roles ---
    case ItemId:             return item.itemId;
    case TransactionId:      return item.transactionId;
    case SenderAvatarUrl:    return item.senderAvatarUrl;
    case ReactionsSummary:   return item.reactionsSummary;
    case PreviousTimestamp: {
        const auto prev = previousVisibleRowFrom(index.row());
        return prev >= 0
            ? static_cast<qulonglong>(items_.at(prev).timestamp)
            : static_cast<qulonglong>(0);
    }
    case PreviousSenderId: {
        const auto prev = previousVisibleRowFrom(index.row());
        return prev >= 0 ? items_.at(prev).senderId : QString();
    }
    case PreviousItemKind: {
        const auto prev = previousVisibleRowFrom(index.row());
        return prev >= 0 ? items_.at(prev).itemKind : QString();
    }
    case DeliveryState:      return item.deliveryState;
    case SendError:          return item.sendError;
    case IsRecoverable:      return item.isRecoverable;
    case IsThreadRoot:       return item.isThreadRoot;
    case ThreadReplyCount:   return static_cast<int>(item.threadReplyCount);
    case IsVoiceMessage:     return item.isVoiceMessage;
    case Waveform: {
        QVariantList list;
        list.reserve(static_cast<int>(item.waveform.size()));
        for (float v : item.waveform)
            list.append(v);
        return list;
    }
    case MessageShield:      return messageShieldFromCode(item.shieldColor, item.shieldCode);
    case MatrixEventType:    return item.matrixEventType;
    case TombstoneReplacementRoomId: return item.tombstoneReplacementRoomId;

    default:                 return {};
    }
    // clang-format on
}

QVariant
MatrixTimelineModel::replyData(const MatrixTimelineItem &parentItem, int role) const
{
    const auto effectiveBody        = effectiveReplyPreviewBody(parentItem);
    const auto effectiveDisplayName = effectiveReplyPreviewDisplayName(parentItem);
    const auto effectiveFormattedBody =
      formatBodyHtml(effectiveBody, parentItem.replyFormattedBody);
    const auto replyType =
      parentItem.replyItemKind.isEmpty() && parentItem.replyMatrixEventType.isEmpty()
        ? static_cast<int>(qml_mtx_events::TextMessage)
        : qml_mtx_events::matrixTimelineEventType(parentItem.replyItemKind,
                                                  parentItem.replyMatrixEventType);
    const auto replyProportionalHeight =
      (parentItem.replyMediaWidth > 0 && parentItem.replyMediaHeight > 0)
        ? static_cast<double>(parentItem.replyMediaHeight) / parentItem.replyMediaWidth
        : 0.0;
    const auto replyFilesize     = parentItem.replyMediaSizeBytes > 0
                                     ? utils::humanReadableFileSize(parentItem.replyMediaSizeBytes)
                                     : QString();
    const auto replyFilename     = parentItem.replyFileName.isEmpty()
                                     ? (effectiveBody.isEmpty() ? QString() : effectiveBody)
                                     : parentItem.replyFileName;
    const auto replyFileTypeIcon = utils::fileTypeIconSource(parentItem.replyMimeType);

    // clang-format off
    switch (role) {
    case Type:               return replyType;
    case TypeString:         return parentItem.replyItemKind.isEmpty() ? QStringLiteral("message")
                                                                       : parentItem.replyItemKind;
    case IsOnlyEmoji:        return utils::emojiOnlyCodepointCount(effectiveBody);
    case Body:               return effectiveBody;
    case FormattedBody:      return effectiveFormattedBody;
    case HasFormattedBody:   return !parentItem.replyFormattedBody.isEmpty();
    case FormattedStateEvent:return QString();
    case StateEventIconSource:return QString();
    case StateEventIconColorCategory:return QString();
    case IsSender:           return false;
    case UserId:             return parentItem.replySenderId;
    case UserName:           return effectiveDisplayName;
    case UserPowerlevel:     return 0;
    case Day:                return 0;
    case Timestamp:          return QDateTime::fromMSecsSinceEpoch(0);
    case Url:                return parentItem.replyMediaUrl;
    case ThumbnailUrl:       return parentItem.replyThumbnailUrl;
    case Duration:           return static_cast<int>(parentItem.replyMediaDurationMs);
    case Blurhash:           return parentItem.replyBlurhash;
    case Filename:           return replyFilename;
    case Filesize:           return replyFilesize;
    case FilesizeBytes:      return static_cast<int>(parentItem.replyMediaSizeBytes);
    case MimeType:           return parentItem.replyMimeType;
    case OriginalHeight:     return static_cast<int>(parentItem.replyMediaHeight);
    case OriginalWidth:      return static_cast<int>(parentItem.replyMediaWidth);
    case ProportionalHeight: return replyProportionalHeight;
    case EventId:            return parentItem.replyEventId;
    case Status:             return 0;
    case IsEdited:           return false;
    case IsEditable:         return false;
    case IsEncrypted:        return false;
    case IsStateEvent:       return false;
    case Trustlevel:         return 0;
    case Notificationlevel:  return 0;
    case UtdCause:           return QString();
    case ReplyTo:            return QString();
    case ThreadId:           return QString();
    case Reactions:          return QVariant();
    case Room:               return false;
    case RoomId:             return QString();
    case CallType:           return QString();
    case Dump:               return QVariant();
    case RelatedEventCacheBuster: return 0;
    case IsHiddenEvent:      return false;
    case FileTypeIconSource: return replyFileTypeIcon;
    case IsVoiceMessage:     return false;
    case Waveform:           return QVariantList{};
    case MessageShield:      return static_cast<int>(crypto::MessageShield::ShieldNone);
    case MatrixEventType:    return parentItem.replyMatrixEventType;
    default:                 return {};
    }
    // clang-format on
}

QHash<int, QByteArray>
MatrixTimelineModel::roleNames() const
{
    return {
      // TimelineModel-compatible roles
      {Type, "type"},
      {TypeString, "typeString"},
      {IsOnlyEmoji, "isOnlyEmoji"},
      {Body, "body"},
      {FormattedBody, "formattedBody"},
      {HasFormattedBody, "hasFormattedBody"},
      {FormattedStateEvent, "formattedStateEvent"},
      {StateEventIconSource, "stateEventIconSource"},
      {IsSender, "isSender"},
      {UserId, "userId"},
      {UserName, "userName"},
      {UserPowerlevel, "userPowerlevel"},
      {Day, "day"},
      {Timestamp, "timestamp"},
      {Url, "url"},
      {ThumbnailUrl, "thumbnailUrl"},
      {Duration, "duration"},
      {Blurhash, "blurhash"},
      {Filename, "filename"},
      {Filesize, "filesize"},
      {FilesizeBytes, "filesizeBytes"},
      {MimeType, "mimetype"},
      {FileTypeIconSource, "fileTypeIconSource"},
      {OriginalHeight, "originalHeight"},
      {OriginalWidth, "originalWidth"},
      {ProportionalHeight, "proportionalHeight"},
      {EventId, "eventId"},
      {Status, "status"},
      {IsEdited, "isEdited"},
      {IsEditable, "isEditable"},
      {IsEncrypted, "isEncrypted"},
      {IsStateEvent, "isStateEvent"},
      {Trustlevel, "trustlevel"},
      {Notificationlevel, "notificationlevel"},
      {UtdCause, "utdCause"},
      {ReplyTo, "replyTo"},
      {ThreadId, "threadId"},
      {Reactions, "reactions"},
      {Room, "room"},
      {RoomId, "roomId"},
      {CallType, "callType"},
      {Dump, "dump"},
      {RelatedEventCacheBuster, "relatedEventCacheBuster"},
      {IsHiddenEvent, "isHiddenEvent"},
      {StateEventIconColorCategory, "stateEventIconColorCategory"},

      // Extra roles
      {ItemId, "itemId"},
      {TransactionId, "transactionId"},
      {SenderAvatarUrl, "senderAvatarUrl"},
      {ReactionsSummary, "reactionsSummary"},
      {PreviousTimestamp, "previousTimestamp"},
      {PreviousSenderId, "previousSenderId"},
      {PreviousItemKind, "previousItemKind"},
      {DeliveryState, "deliveryState"},
      {SendError, "sendError"},
      {IsRecoverable, "isRecoverable"},
      {IsThreadRoot, "isThreadRoot"},
      {ThreadReplyCount, "threadReplyCount"},
      {IsVoiceMessage, "isVoiceMessage"},
      {Waveform, "waveform"},
      {MessageShield, "messageShield"},
      {MatrixEventType, "matrixEventType"},
      {TombstoneReplacementRoomId, "tombstoneReplacementRoomId"},
      {IsLatestCallNotification, "isLatestCallNotification"},
    };
}

// --- EventDataSource interface ---

QVariant
MatrixTimelineModel::dataById(const QString &id, int role, const QString &relatedTo)
{
    if (id.isEmpty())
        return {};

    // Look up the item directly by its event/item ID.
    const auto row = rowForEventId(id);
    if (row >= 0 && row < items_.size())
        return data(index(row), role);

    // If not found as a standalone item but we have a parent event,
    // return inline reply data from the parent.
    if (!relatedTo.isEmpty()) {
        const auto parentRow = rowForEventIdInItems(allItems_, relatedTo);
        if (parentRow >= 0 && parentRow < allItems_.size())
            return replyData(allItems_.at(parentRow), role);
    }

    return {};
}

void
MatrixTimelineModel::multiData(const QString &id,
                               const QString &relatedTo,
                               QModelRoleDataSpan roleDataSpan) const
{
    if (id.isEmpty()) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    const auto row = rowForEventId(id);
    if (row >= 0 && row < items_.size()) {
        const auto idx = index(row);
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.setData(data(idx, roleData.role()));
        return;
    }

    // Reply fallback: return inline reply data from the parent event.
    if (!relatedTo.isEmpty()) {
        const auto parentRow = rowForEventIdInItems(allItems_, relatedTo);
        if (parentRow >= 0 && parentRow < allItems_.size()) {
            const auto &parentItem = allItems_.at(parentRow);
            for (QModelRoleData &roleData : roleDataSpan)
                roleData.setData(replyData(parentItem, roleData.role()));
            return;
        }
    }

    for (QModelRoleData &roleData : roleDataSpan)
        roleData.clearData();
}

int
MatrixTimelineModel::idToIndex(const QString &id) const
{
    return rowForEventId(id);
}

int
MatrixTimelineModel::rowForEventId(const QString &eventId) const
{
    return rowForEventIdInItems(items_, eventId);
}

int
MatrixTimelineModel::rawRowForEventId(const QString &eventId) const
{
    return rowForEventIdInItems(allItems_, eventId);
}

int
MatrixTimelineModel::rowForEventIdInItems(const QVector<MatrixTimelineItem> &items,
                                          const QString &eventId) const
{
    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return -1;

    for (int row = 0; row < items.size(); ++row) {
        const auto &item = items.at(row);
        if (item.eventId == trimmedEventId || item.itemId == trimmedEventId)
            return row;
    }

    return -1;
}

QVariantMap
MatrixTimelineModel::itemAt(int row) const
{
    QVariantMap itemData;

    if (row < 0 || row >= items_.size())
        return itemData;

    const auto roleNameMap = roleNames();
    const auto itemIndex   = index(row);
    for (auto it = roleNameMap.cbegin(); it != roleNameMap.cend(); ++it) {
        itemData.insert(QString::fromUtf8(it.value()), data(itemIndex, it.key()));
    }

    return itemData;
}

QVariantMap
MatrixTimelineModel::previewDataForEvent(const QString &eventId, const QString &relatedTo) const
{
    QVariantMap previewData;
    const auto normalizedEventId   = eventId.trimmed();
    const auto normalizedRelatedTo = relatedTo.trimmed();

    auto insertReplyRole = [&previewData](const QString &key, const QVariant &value) {
        previewData.insert(key, value);
    };

    auto insertItemPreview = [&previewData](const MatrixTimelineItem &item) {
        previewData.insert(QStringLiteral("eventId"), item.eventId);
        previewData.insert(QStringLiteral("type"), item.cachedType);
        previewData.insert(QStringLiteral("typeString"), item.itemKind);
        previewData.insert(QStringLiteral("userId"), item.senderId);
        previewData.insert(QStringLiteral("userName"), item.senderDisplayName);
        previewData.insert(QStringLiteral("avatarUrl"), item.senderAvatarUrl);
        previewData.insert(QStringLiteral("body"), item.body);
        previewData.insert(QStringLiteral("formattedBody"), item.cachedFormattedBody);
        previewData.insert(QStringLiteral("formattedStateEvent"), item.cachedFormattedStateEvent);
        previewData.insert(QStringLiteral("stateEventIconSource"), item.cachedStateEventIcon);
        previewData.insert(QStringLiteral("stateEventIconColorCategory"),
                           item.cachedStateEventIconColorCategory);
        previewData.insert(QStringLiteral("isOnlyEmoji"), item.cachedEmojiOnlyCount);
        previewData.insert(QStringLiteral("url"), item.mediaUrl);
        previewData.insert(QStringLiteral("thumbnailUrl"), item.thumbnailUrl);
        previewData.insert(QStringLiteral("duration"),
                           static_cast<qulonglong>(item.mediaDurationMs));
        previewData.insert(QStringLiteral("blurhash"), QString());
        previewData.insert(QStringLiteral("filename"), item.cachedFilename);
        previewData.insert(QStringLiteral("filesize"), item.cachedFilesize);
        previewData.insert(QStringLiteral("filesizeBytes"),
                           static_cast<qulonglong>(item.mediaSizeBytes));
        previewData.insert(QStringLiteral("mimetype"), item.mimeType);
        previewData.insert(QStringLiteral("fileTypeIconSource"), item.cachedFileTypeIcon);
        previewData.insert(QStringLiteral("originalHeight"),
                           static_cast<qulonglong>(item.mediaHeight));
        previewData.insert(QStringLiteral("originalWidth"),
                           static_cast<qulonglong>(item.mediaWidth));
        previewData.insert(QStringLiteral("proportionalHeight"), item.cachedProportionalH);
        previewData.insert(QStringLiteral("callType"), QString());
        previewData.insert(QStringLiteral("isEdited"), item.isEdited);
        previewData.insert(QStringLiteral("isEditable"), item.cachedIsEditable);
        previewData.insert(QStringLiteral("isEncrypted"), item.cachedIsEncrypted);
        previewData.insert(QStringLiteral("isStateEvent"), item.cachedIsStateEvent);
        previewData.insert(QStringLiteral("messageShield"),
                           messageShieldFromCode(item.shieldColor, item.shieldCode));
        previewData.insert(QStringLiteral("replyTo"),
                           item.cachedIsStateEvent ? QString() : item.replyEventId);
        previewData.insert(QStringLiteral("threadId"), item.threadId);
    };

    if (!normalizedEventId.isEmpty()) {
        const auto directRow = rowForEventIdInItems(allItems_, normalizedEventId);
        if (directRow >= 0 && directRow < allItems_.size()) {
            insertItemPreview(allItems_.at(directRow));
            return previewData;
        }
    }

    if (normalizedEventId.isEmpty() || normalizedRelatedTo.isEmpty())
        return previewData;

    const auto parentRow = rowForEventIdInItems(allItems_, normalizedRelatedTo);
    if (parentRow < 0 || parentRow >= allItems_.size())
        return previewData;

    const auto &parentItem = allItems_.at(parentRow);
    insertReplyRole(QStringLiteral("eventId"), replyData(parentItem, EventId));
    insertReplyRole(QStringLiteral("type"), replyData(parentItem, Type));
    insertReplyRole(QStringLiteral("typeString"), replyData(parentItem, TypeString));
    insertReplyRole(QStringLiteral("userId"), replyData(parentItem, UserId));
    insertReplyRole(QStringLiteral("userName"), replyData(parentItem, UserName));
    insertReplyRole(QStringLiteral("avatarUrl"), QString());
    insertReplyRole(QStringLiteral("body"), replyData(parentItem, Body));
    insertReplyRole(QStringLiteral("formattedBody"), replyData(parentItem, FormattedBody));
    insertReplyRole(QStringLiteral("formattedStateEvent"),
                    replyData(parentItem, FormattedStateEvent));
    insertReplyRole(QStringLiteral("stateEventIconSource"),
                    replyData(parentItem, StateEventIconSource));
    insertReplyRole(QStringLiteral("stateEventIconColorCategory"),
                    replyData(parentItem, StateEventIconColorCategory));
    insertReplyRole(QStringLiteral("isOnlyEmoji"), replyData(parentItem, IsOnlyEmoji));
    insertReplyRole(QStringLiteral("url"), replyData(parentItem, Url));
    insertReplyRole(QStringLiteral("thumbnailUrl"), replyData(parentItem, ThumbnailUrl));
    insertReplyRole(QStringLiteral("duration"), replyData(parentItem, Duration));
    insertReplyRole(QStringLiteral("blurhash"), replyData(parentItem, Blurhash));
    insertReplyRole(QStringLiteral("filename"), replyData(parentItem, Filename));
    insertReplyRole(QStringLiteral("filesize"), replyData(parentItem, Filesize));
    insertReplyRole(QStringLiteral("filesizeBytes"), replyData(parentItem, FilesizeBytes));
    insertReplyRole(QStringLiteral("mimetype"), replyData(parentItem, MimeType));
    insertReplyRole(QStringLiteral("fileTypeIconSource"),
                    replyData(parentItem, FileTypeIconSource));
    insertReplyRole(QStringLiteral("originalHeight"), replyData(parentItem, OriginalHeight));
    insertReplyRole(QStringLiteral("originalWidth"), replyData(parentItem, OriginalWidth));
    insertReplyRole(QStringLiteral("proportionalHeight"),
                    replyData(parentItem, ProportionalHeight));
    insertReplyRole(QStringLiteral("callType"), replyData(parentItem, CallType));
    insertReplyRole(QStringLiteral("isEdited"), replyData(parentItem, IsEdited));
    insertReplyRole(QStringLiteral("isEditable"), replyData(parentItem, IsEditable));
    insertReplyRole(QStringLiteral("isEncrypted"), replyData(parentItem, IsEncrypted));
    insertReplyRole(QStringLiteral("isStateEvent"), replyData(parentItem, IsStateEvent));
    insertReplyRole(QStringLiteral("replyTo"), replyData(parentItem, ReplyTo));
    insertReplyRole(QStringLiteral("threadId"), replyData(parentItem, ThreadId));
    insertReplyRole(QStringLiteral("isVoiceMessage"), replyData(parentItem, IsVoiceMessage));
    insertReplyRole(QStringLiteral("waveform"), replyData(parentItem, Waveform));
    insertReplyRole(QStringLiteral("messageShield"), replyData(parentItem, MessageShield));
    return previewData;
}

QString
MatrixTimelineModel::avatarUrl(const QString &userId) const
{
    if (userId.isEmpty())
        return {};

    for (const auto &item : allItems_) {
        if (item.senderId == userId && !item.senderAvatarUrl.isEmpty())
            return item.senderAvatarUrl;
    }

    return {};
}

::rust::Vec<::komai::rust::HtmlPillAvatar>
MatrixTimelineModel::buildPillAvatars(const QVector<MatrixTimelineItem> &items) const
{
    ::rust::Vec<::komai::rust::HtmlPillAvatar> avatars;

    // Last-write-wins per sender: the latest item carries the most current
    // displayName/avatarUrl snapshot. Using the first occurrence (as the
    // previous implementation did) leaves stale mxc URLs in the map after a
    // user clears or rotates their avatar, producing the broken-image gap
    // in pills described in the bug report.
    struct PillSourceFields
    {
        QString displayName;
        QString avatarUrl;
    };
    QHash<QString, PillSourceFields> bySender;
    QStringList senderOrder;
    for (const auto &item : items) {
        if (item.senderId.isEmpty())
            continue;
        if (!bySender.contains(item.senderId))
            senderOrder.push_back(item.senderId);
        bySender[item.senderId] = {item.senderDisplayName, item.senderAvatarUrl};
    }

    auto *timeline       = TimelineViewManager::instance();
    const auto settings  = UserSettings::instance();
    const int style      = settings ? static_cast<int>(settings->uiAvatarsDefaultAvatarStyle()) : 0;
    const int radius     = (settings && settings->uiAvatarsCircular()) ? 100 : 25;
    const auto baseColor = QGuiApplication::palette().color(QPalette::Base);

    for (const auto &senderId : senderOrder) {
        const auto &fields = bySender.value(senderId);

        QString mxcUrl;
        if (fields.avatarUrl.startsWith(QLatin1String("mxc://")))
            mxcUrl = fields.avatarUrl;

        QString fallbackUrl;
        if (timeline) {
            const auto color       = timeline->roomUserColor(roomId_, senderId, baseColor, -1);
            const QString colorHex = color.isValid() ? color.name().mid(1) : QString();
            const auto encodedName = QString::fromUtf8(QUrl::toPercentEncoding(fields.displayName));
            // Mirror the URL shape Avatar.qml builds (including `_v` cache-buster
            // tied to the avatar style) so the DefaultAvatarProvider picks up
            // setting changes without stale-cache artefacts.
            fallbackUrl =
              QStringLiteral("image://default-avatar/%1?radius=%2&displayName=%3&"
                             "color=%4&style=%5&_v=%5")
                .arg(
                  senderId, QString::number(radius), encodedName, colorHex, QString::number(style));
        }

        ::komai::rust::HtmlPillAvatar entry;
        entry.user_id      = ::rust::String(senderId.toStdString());
        entry.mxc_url      = ::rust::String(mxcUrl.toStdString());
        entry.fallback_url = ::rust::String(fallbackUrl.toStdString());
        avatars.push_back(std::move(entry));
    }

    return avatars;
}

QString
MatrixTimelineModel::copyTextForEventIds(const QVariantList &eventIds, bool plainText) const
{
    struct CopiedEntry
    {
        QString header;
        QString text;
    };
    QList<CopiedEntry> entries;
    entries.reserve(static_cast<qsizetype>(eventIds.size()));

    for (const auto &eventIdValue : eventIds) {
        const auto eventId = eventIdValue.toString().trimmed();
        if (eventId.isEmpty())
            continue;

        const auto item = itemByEventId(eventId);
        if (!item)
            continue;

        const auto copiedText =
          plainText ? plainCopyTextForItem(*item) : originalCopyTextForItem(*item);
        if (copiedText.isEmpty())
            continue;

        entries.push_back({copyHeaderForItem(*item), copiedText});
    }

    // Single-message copy stays bare so pasted bodies (snippets, URLs, commands) carry
    // no attribution noise. Multi-message copy needs headers to be readable as a transcript.
    const bool multi = entries.size() > 1;
    QStringList joined;
    joined.reserve(entries.size());
    for (const auto &entry : entries)
        joined.push_back(multi ? entry.header + entry.text : entry.text);

    return joined.join(multi ? QStringLiteral("\n\n--------\n\n") : QStringLiteral("\n\n"));
}

QString
MatrixTimelineModel::userNameForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    return item ? item->senderDisplayName : QString();
}

QString
MatrixTimelineModel::userIdForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    return item ? item->senderId : QString();
}

QString
MatrixTimelineModel::bodyForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    return item ? item->body : QString();
}

QString
MatrixTimelineModel::typeStringForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    return item ? item->itemKind : QString();
}

QString
MatrixTimelineModel::filenameForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    if (!item)
        return {};
    if (!item->fileName.isEmpty())
        return item->fileName;
    if (!item->body.isEmpty())
        return item->body;
    return {};
}

std::optional<MatrixTimelineItem>
MatrixTimelineModel::itemByEventId(const QString &eventId) const
{
    const auto row = rowForEventIdInItems(allItems_, eventId);
    if (row < 0 || row >= allItems_.size())
        return std::nullopt;

    return allItems_.at(row);
}

int
MatrixTimelineModel::hiddenCount() const
{
    return std::max(0, static_cast<int>(allItems_.size() - revealedItemCount_));
}

void
MatrixTimelineModel::applyRedactedPresentation(MatrixTimelineItem &item) const
{
    item.body.clear();
    item.replyEventId.clear();
    item.replySenderId.clear();
    item.replySenderDisplayName.clear();
    item.replyBody.clear();
    item.reactions.clear();
    item.reactionsSummary.clear();
    item.itemKind = QStringLiteral("redacted");
    item.membershipChangeKind.clear();
    item.isEdited = false;
    item.mediaUrl.clear();
    item.thumbnailUrl.clear();
    item.fileName.clear();
    item.mimeType.clear();
    item.mediaWidth           = 0;
    item.mediaHeight          = 0;
    item.mediaDurationMs      = 0;
    item.mediaSizeBytes       = 0;
    item.mediaIsEncrypted     = false;
    item.thumbnailIsEncrypted = false;
    item.isVoiceMessage       = false;
    item.waveform.clear();
    computeDerivedFields(item, roomId_);
}

bool
MatrixTimelineModel::redactItemByEventId(const QString &eventId)
{
    const auto normalizedEventId = eventId.trimmed();
    const auto row               = rowForEventId(normalizedEventId);
    if (row < 0 || row >= items_.size())
        return false;

    optimisticRedactedEventIds_.insert(normalizedEventId);

    auto &item = items_[row];
    applyRedactedPresentation(item);

    emit dataChanged(index(row), index(row));
    return true;
}

bool
MatrixTimelineModel::removeItemByTransactionId(const QString &transactionId)
{
    const auto normalized = transactionId.trimmed();
    if (normalized.isEmpty())
        return false;

    int visibleRow = -1;
    for (int row = 0; row < items_.size(); ++row) {
        const auto &it = items_.at(row);
        if (it.transactionId == normalized || it.itemId == normalized) {
            visibleRow = row;
            break;
        }
    }

    int rawRow = -1;
    for (int row = 0; row < allItems_.size(); ++row) {
        const auto &it = allItems_.at(row);
        if (it.transactionId == normalized || it.itemId == normalized) {
            rawRow = row;
            break;
        }
    }

    if (rawRow < 0 && visibleRow < 0)
        return false;

    if (visibleRow >= 0) {
        beginRemoveRows({}, visibleRow, visibleRow);
        items_.removeAt(visibleRow);
        endRemoveRows();
    }

    if (rawRow >= 0) {
        allItems_.removeAt(rawRow);
        if (revealedItemCount_ > allItems_.size())
            revealedItemCount_ = allItems_.size();
    }

    emit countChanged();
    if (rawRow >= 0)
        emit rawCountChanged();
    return true;
}

bool
MatrixTimelineModel::revealOlderItems(int additionalCount)
{
    const auto availableHiddenCount = hiddenCount();
    if (availableHiddenCount <= 0)
        return false;

    const auto revealCount = std::clamp(
      additionalCount > 0 ? additionalCount : availableHiddenCount, 1, availableHiddenCount);
    const auto oldVisibleCount = items_.size();
    revealedItemCount_ =
      std::min(static_cast<int>(allItems_.size()), revealedItemCount_ + revealCount);
    replaceVisibleItems(visibleItemsForRawCount(revealedItemCount_));

    // "countChanged" also acts as the TimelineManager state nudge used by
    // pending-jump recovery and viewport bookkeeping. Emit it even when the
    // visible row count stays flat because the newly revealed raw items are
    // all hidden by local preferences.
    if (items_.size() == oldVisibleCount)
        emit countChanged();
    return true;
}

void
MatrixTimelineModel::applyOptimisticRedactions(QVector<MatrixTimelineItem> &items)
{
    if (optimisticRedactedEventIds_.isEmpty())
        return;

    QSet<QString> resolvedEventIds;
    for (auto &item : items) {
        const auto eventId = item.eventId.trimmed();
        const auto itemId  = item.itemId.trimmed();
        if (!optimisticRedactedEventIds_.contains(eventId) &&
            !optimisticRedactedEventIds_.contains(itemId)) {
            continue;
        }

        if (item.itemKind == QStringLiteral("redacted")) {
            if (!eventId.isEmpty())
                resolvedEventIds.insert(eventId);
            if (!itemId.isEmpty())
                resolvedEventIds.insert(itemId);
            continue;
        }

        applyRedactedPresentation(item);
    }

    for (const auto &resolvedEventId : resolvedEventIds)
        optimisticRedactedEventIds_.remove(resolvedEventId);
}

void
MatrixTimelineModel::refreshDerivedFields()
{
    if (allItems_.isEmpty()) {
        revealedItemCount_ = 0;
        replaceVisibleItems({});
        return;
    }

    const auto avatars = buildPillAvatars(allItems_);
    for (auto &item : allItems_)
        computeDerivedFields(item, roomId_, avatars);

    revealedItemCount_ = std::clamp(revealedItemCount_, 0, static_cast<int>(allItems_.size()));
    replaceVisibleItems(visibleItemsForRawCount(revealedItemCount_));
}

int
MatrixTimelineModel::previousVisibleRowFrom(int row) const
{
    for (int candidate = row + 1; candidate < items_.size(); ++candidate) {
        if (!items_.at(candidate).cachedIsHiddenEvent)
            return candidate;
    }

    return -1;
}

QVector<MatrixTimelineItem>
MatrixTimelineModel::visibleItemsForRawCount(int rawVisibleCount) const
{
    QVector<MatrixTimelineItem> visibleItems;
    const auto clampedRawVisibleCount =
      std::clamp(rawVisibleCount, 0, static_cast<int>(allItems_.size()));
    visibleItems.reserve(clampedRawVisibleCount);

    for (int i = 0; i < clampedRawVisibleCount; ++i) {
        const auto &item = allItems_.at(i);
        if (!item.cachedIsHiddenEvent)
            visibleItems.push_back(item);
    }

    return visibleItems;
}

void
MatrixTimelineModel::replaceVisibleItems(QVector<MatrixTimelineItem> items)
{
    if (items_ == items)
        return;

    const auto oldCount       = items_.size();
    const auto newCount       = items.size();
    const bool countDidChange = oldCount != newCount;

    auto prefix                = 0;
    const auto comparableCount = std::min(oldCount, newCount);
    while (prefix < comparableCount && items_.at(prefix) == items.at(prefix))
        ++prefix;

    auto oldSuffix = oldCount - 1;
    auto newSuffix = newCount - 1;
    while (oldSuffix >= prefix && newSuffix >= prefix &&
           items_.at(oldSuffix) == items.at(newSuffix)) {
        --oldSuffix;
        --newSuffix;
    }

    const auto oldChanged = oldSuffix - prefix + 1;
    const auto newChanged = newSuffix - prefix + 1;

    if (oldChanged == 0 && newChanged > 0) {
        beginInsertRows({}, prefix, prefix + newChanged - 1);
        items_ = std::move(items);
        endInsertRows();
    } else if (newChanged == 0 && oldChanged > 0) {
        beginRemoveRows({}, prefix, prefix + oldChanged - 1);
        items_ = std::move(items);
        endRemoveRows();
    } else if (oldChanged == newChanged) {
        items_ = std::move(items);
        if (oldChanged > 0)
            emit dataChanged(index(prefix), index(prefix + oldChanged - 1));
    } else {
        // Use a full model reset for mixed remove+insert changes.
        // The previous decomposed remove+insert approach (endRemoveRows
        // followed by endInsertRows) could trigger a Qt 6 incremental GC
        // bug: delegate destruction during endRemoveRows freed V4 heap
        // objects whose stale references remained on the GC worklist,
        // causing SIGSEGV in QV4::GCStateMachine::transition() when the
        // GC later followed pointers into 0xAA-filled freed memory.
        //
        // beginResetModel resets the ListView's contentY in BottomToTop
        // mode, so we emit aboutToReplaceContent/contentReplaced to let
        // the QML side save and restore the scroll position.
        emit aboutToReplaceContent();
        beginResetModel();
        items_ = std::move(items);
        endResetModel();
        emit contentReplaced();
    }

    // Bubble grouping/section/avatar layout for row N depends on the next visible row (N + 1).
    // When hidden-event filtering inserts/removes rows, the unchanged boundary row just before the
    // diff can keep stale grouping state unless we nudge it explicitly.
    if (prefix > 0 && prefix - 1 < items_.size())
        emit dataChanged(index(prefix - 1), index(prefix - 1));

    if (countDidChange)
        emit countChanged();
}

void
MatrixTimelineModel::replaceItems(QVector<MatrixTimelineItem> items)
{
    applyOptimisticRedactions(items);

    // Filter out date_divider items — the bubble style's built-in section
    // headers handle date display, so keeping date dividers as model rows
    // just wastes delegate instantiations.
    items.erase(std::remove_if(items.begin(),
                               items.end(),
                               [](const MatrixTimelineItem &item) {
                                   return item.itemKind == QStringLiteral("date_divider");
                               }),
                items.end());

    // Defensive dedup by stable identity. matrix-sdk-ui owns uniqueness of
    // `unique_id` (→ `itemId`) and Matrix event_id (→ `eventId`) within a
    // single timeline vector, but a slipped local-echo→remote-echo merge can
    // leave two entries pointing at the same event: we'd render the same
    // bubble twice AND inflate `allItems_.size()` past the visible-window
    // cap, pushing genuine later rows out of the capped `items_` view (they
    // appear "missing"). Drop the later occurrences so the snapshot is at
    // least internally consistent until the next SDK update arrives.
    {
        QSet<QString> seenEventIds;
        QSet<QString> seenItemIds;
        int droppedCount = 0;
        items.erase(std::remove_if(items.begin(),
                                   items.end(),
                                   [&](const MatrixTimelineItem &item) {
                                       const auto eventId = item.eventId.trimmed();
                                       const auto itemId  = item.itemId.trimmed();
                                       bool isDuplicate   = false;
                                       if (!eventId.isEmpty() && seenEventIds.contains(eventId))
                                           isDuplicate = true;
                                       if (!itemId.isEmpty() && seenItemIds.contains(itemId))
                                           isDuplicate = true;
                                       if (isDuplicate) {
                                           ++droppedCount;
                                           return true;
                                       }
                                       if (!eventId.isEmpty())
                                           seenEventIds.insert(eventId);
                                       if (!itemId.isEmpty())
                                           seenItemIds.insert(itemId);
                                       return false;
                                   }),
                    items.end());
        if (droppedCount > 0) {
            komai::logging::ui()->warn(
              "MatrixTimelineModel: dropped {} duplicate timeline item(s) from snapshot for "
              "room '{}'; matrix-sdk-ui delivered overlapping entries",
              droppedCount,
              roomId_.toStdString());
        }
    }

    const auto avatars = buildPillAvatars(items);
    for (auto &item : items)
        computeDerivedFields(item, roomId_, avatars);

    // Derive isThreadRoot: collect all threadId values (these reference the
    // thread root event), then mark items whose eventId appears in that set.
    // The SDK's thread_summary field would be the canonical source for this,
    // but it is not populated on timeline snapshots from the local store,
    // so we derive it from sibling items instead.
    {
        QSet<QString> threadRootIds;
        for (const auto &item : items) {
            if (!item.threadId.isEmpty())
                threadRootIds.insert(item.threadId);
        }
        for (auto &item : items) {
            if (!item.isThreadRoot && !item.eventId.isEmpty() &&
                threadRootIds.contains(item.eventId)) {
                item.isThreadRoot = true;
            }
        }
    }

    emitEffectsForPrependedItems(items);
    const auto oldRawCount = allItems_.size();
    allItems_              = items;
    const auto newRawCount = allItems_.size();

    const auto initialVisibleWindow      = configuredInitialVisibleWindow();
    const auto uncappedVisibleCount      = static_cast<int>(newRawCount);
    const auto cappedInitialVisibleCount = initialVisibleWindow
                                             ? std::min(uncappedVisibleCount, *initialVisibleWindow)
                                             : uncappedVisibleCount;

    auto targetVisibleCount = cappedInitialVisibleCount;
    if (initialVisibleWindow && revealedItemCount_ > 0)
        targetVisibleCount = std::max(revealedItemCount_, cappedInitialVisibleCount);

    revealedItemCount_ = std::clamp(targetVisibleCount, 0, static_cast<int>(newRawCount));
    replaceVisibleItems(visibleItemsForRawCount(revealedItemCount_));

    refreshLatestRtcNotification();

    if (newRawCount != oldRawCount)
        emit rawCountChanged();
}

void
MatrixTimelineModel::refreshLatestRtcNotification()
{
    // Pick the newest `m.rtc.notification` (Element Call "a call started") so the
    // call tile shows live ongoing/ended + Join state on only the latest one.
    QString latestEventId;
    uint64_t latestTimestamp = 0;
    for (const auto &item : allItems_) {
        if (item.cachedType != qml_mtx_events::CallNotification || item.eventId.isEmpty())
            continue;
        if (latestEventId.isEmpty() || item.timestamp >= latestTimestamp) {
            latestTimestamp = item.timestamp;
            latestEventId   = item.eventId;
        }
    }

    if (latestEventId != latestRtcNotificationEventId_) {
        latestRtcNotificationEventId_ = latestEventId;
        emit latestRtcNotificationEventIdChanged();

        // Refresh the IsLatestCallNotification role on the visible call rows so
        // the delegate re-reads it (the old latest reverts to historical, the
        // new latest goes live).
        for (int row = 0; row < items_.size(); ++row) {
            if (items_.at(row).cachedType == qml_mtx_events::CallNotification) {
                const auto idx = index(row);
                emit dataChanged(idx, idx, {IsLatestCallNotification});
            }
        }
    }
}

void
MatrixTimelineModel::emitEffectsForPrependedItems(const QVector<MatrixTimelineItem> &nextItems)
{
    if (allItems_.isEmpty() || nextItems.isEmpty())
        return;

    const auto oldHeadKey = stableTimelineItemKey(allItems_.front());
    if (oldHeadKey.isEmpty())
        return;

    int prependCount = -1;
    for (int i = 0; i < nextItems.size(); ++i) {
        if (stableTimelineItemKey(nextItems.at(i)) == oldHeadKey) {
            prependCount = i;
            break;
        }
    }

    if (prependCount <= 0)
        return;

    QStringList effectNames;
    effectNames.reserve(prependCount);
    QSet<QString> seenEffects;
    for (int i = prependCount - 1; i >= 0; --i) {
        const auto &item = nextItems.at(i);
        for (const auto &effectName : item.specialEffectNames) {
            const auto trimmed = effectName.trimmed();
            if (trimmed.isEmpty() || seenEffects.contains(trimmed))
                continue;
            seenEffects.insert(trimmed);
            effectNames.push_back(trimmed);
        }
    }

    if (!effectNames.isEmpty())
        emit specialEffectsTriggered(effectNames);
}

void
MatrixTimelineModel::clear()
{
    optimisticRedactedEventIds_.clear();
    const auto hadRawItems = !allItems_.isEmpty();
    allItems_.clear();
    revealedItemCount_ = 0;
    replaceVisibleItems({});
    refreshLatestRtcNotification();
    if (hadRawItems)
        emit rawCountChanged();
}

} // namespace komai

#include "moc_MatrixTimelineModel.cpp"
