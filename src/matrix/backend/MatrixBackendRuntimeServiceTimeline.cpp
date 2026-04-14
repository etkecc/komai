// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QStringList>

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "matrix/backend/MatrixBackendRuntimeServiceInternal.h"
#include "matrix/backend/MatrixBlockingCall.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "timeline/Reaction.h"
#include "voip/CallTypes.h"

namespace komai {

namespace {

bool
isTruthyEnvValue(const QByteArray &value)
{
    const auto normalized = value.trimmed().toLower();
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

bool
roomSwitchPerfEnabled()
{
    return isTruthyEnvValue(qgetenv("KOMAI_ROOM_SWITCH_PERF")) ||
           isTruthyEnvValue(qgetenv("KOMAI_PERF_ROOM_SWITCH"));
}

MatrixReadReceiptEntry
fromRustReadReceiptEntry(const ::komai::rust::MatrixReadReceiptEntry &entry)
{
    return MatrixReadReceiptEntry{
      .userId      = QString::fromStdString(std::string(entry.user_id)),
      .displayName = QString::fromStdString(std::string(entry.display_name)),
      .avatarUrl   = matrix::normalizeMxcUri(QString::fromStdString(std::string(entry.avatar_url))),
      .timestamp   = entry.timestamp,
    };
}

MatrixTimelineItem
fromRustTimelineItem(const ::komai::rust::MatrixTimelineItem &item)
{
    QVariantList reactions;
    reactions.reserve(static_cast<qsizetype>(item.reactions.size()));
    for (const auto &reaction : item.reactions) {
        Reaction value;
        value.key_              = QString::fromStdString(std::string(reaction.key));
        value.users_            = QString::fromStdString(std::string(reaction.users));
        value.selfReactedEvent_ = QString::fromStdString(std::string(reaction.self_reacted_event));
        value.count_            = static_cast<int>(reaction.count);
        reactions.push_back(QVariant::fromValue(value));
    }

    QStringList specialEffectNames;
    specialEffectNames.reserve(static_cast<qsizetype>(item.special_effect_names.size()));
    for (const auto &effectName : item.special_effect_names)
        specialEffectNames.push_back(QString::fromStdString(std::string(effectName)));

    return MatrixTimelineItem{
      .itemId            = QString::fromStdString(std::string(item.item_id)),
      .eventId           = QString::fromStdString(std::string(item.event_id)),
      .deliveryState     = QString::fromStdString(std::string(item.delivery_state)),
      .threadId          = QString::fromStdString(std::string(item.thread_id)),
      .isThreadRoot      = item.is_thread_root,
      .senderId          = QString::fromStdString(std::string(item.sender_id)),
      .senderDisplayName = QString::fromStdString(std::string(item.sender_display_name)),
      .senderAvatarUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.sender_avatar_url))),
      .body                   = QString::fromStdString(std::string(item.body)),
      .formattedBody          = QString::fromStdString(std::string(item.formatted_body)),
      .replyEventId           = QString::fromStdString(std::string(item.reply_event_id)),
      .replySenderId          = QString::fromStdString(std::string(item.reply_sender_id)),
      .replySenderDisplayName = QString::fromStdString(std::string(item.reply_sender_display_name)),
      .replyItemKind          = QString::fromStdString(std::string(item.reply_item_kind)),
      .replyMatrixEventType   = QString::fromStdString(std::string(item.reply_matrix_event_type)),
      .replyBody              = QString::fromStdString(std::string(item.reply_body)),
      .replyFormattedBody     = QString::fromStdString(std::string(item.reply_formatted_body)),
      .replyMediaUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.reply_media_url))),
      .replyThumbnailUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.reply_thumbnail_url))),
      .replyFileName        = QString::fromStdString(std::string(item.reply_file_name)),
      .replyMimeType        = QString::fromStdString(std::string(item.reply_mime_type)),
      .replyMediaWidth      = item.reply_media_width,
      .replyMediaHeight     = item.reply_media_height,
      .replyMediaDurationMs = item.reply_media_duration_ms,
      .replyMediaSizeBytes  = item.reply_media_size_bytes,
      .replyBlurhash        = QString::fromStdString(std::string(item.reply_blurhash)),
      .reactions            = reactions,
      .reactionsSummary     = QString::fromStdString(std::string(item.reactions_summary)),
      .specialEffectNames   = specialEffectNames,
      .itemKind             = QString::fromStdString(std::string(item.item_kind)),
      .membershipChangeKind = QString::fromStdString(std::string(item.membership_change_kind)),
      .matrixEventType      = QString::fromStdString(std::string(item.matrix_event_type)),
      .isEdited             = item.is_edited,
      .mediaUrl = matrix::normalizeMxcUri(QString::fromStdString(std::string(item.media_url))),
      .thumbnailUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.thumbnail_url))),
      .fileName             = QString::fromStdString(std::string(item.file_name)),
      .mimeType             = QString::fromStdString(std::string(item.mime_type)),
      .mediaWidth           = item.media_width,
      .mediaHeight          = item.media_height,
      .mediaDurationMs      = item.media_duration_ms,
      .mediaSizeBytes       = item.media_size_bytes,
      .blurhash             = QString::fromStdString(std::string(item.blurhash)),
      .mediaIsEncrypted     = item.media_is_encrypted,
      .thumbnailIsEncrypted = item.thumbnail_is_encrypted,
      .isVoiceMessage       = item.is_voice_message,
      .waveform =
        [&item]() {
            QList<float> list;
            list.reserve(static_cast<int>(item.waveform.size()));
            for (float v : item.waveform)
                list.append(v);
            return list;
        }(),
      .timestamp            = item.timestamp,
      .isOwn                = item.is_own,
      .stateEventTargetUser = QString::fromStdString(std::string(item.state_event_target_user)),
      .stateEventTargetUserId =
        QString::fromStdString(std::string(item.state_event_target_user_id)),
      .stateEventDetail    = QString::fromStdString(std::string(item.state_event_detail)),
      .stateEventReason    = QString::fromStdString(std::string(item.state_event_reason)),
      .stateEventHasSender = item.state_event_has_sender,
      .powerLevelChanges =
        [&item]() {
            QList<PowerLevelChange> list;
            list.reserve(static_cast<int>(item.power_level_changes.size()));
            for (const auto &change : item.power_level_changes)
                list.append(PowerLevelChange{
                  .userId   = QString::fromStdString(std::string(change.user_id)),
                  .oldLevel = change.old_level,
                  .newLevel = change.new_level,
                });
            return list;
        }(),
      .cachedType                = 0,
      .cachedEmojiOnlyCount      = 0,
      .cachedDay                 = 0,
      .cachedStatus              = 0,
      .cachedIsStateEvent        = false,
      .cachedIsEncrypted         = false,
      .cachedIsEditable          = false,
      .cachedProportionalH       = 0.0,
      .cachedFormattedBody       = QString(),
      .cachedFormattedStateEvent = QString(),
      .cachedStateEventIcon      = QString(),
      .cachedFilesize            = QString(),
      .cachedFilename            = QString(),
      .cachedFileTypeIcon        = QString(),
    };
}

} // anonymous namespace

bool
MatrixBackendRuntimeService::selectActiveRoomTimeline(uint64_t handleId,
                                                      const QString &roomId,
                                                      QString *errorOut)
{
    try {
        ::komai::rust::matrix_select_active_room_timeline(handleId, roomId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::stopRoomTimeline(uint64_t handleId,
                                              const QString &roomId,
                                              QString *errorOut)
{
    try {
        ::komai::rust::matrix_stop_room_timeline(handleId, roomId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QVector<MatrixTimelineItem>>
MatrixBackendRuntimeService::fetchRoomTimelineSnapshot(matrix_backend::BlockingCallContext context,
                                                       uint64_t handleId,
                                                       const QString &roomId,
                                                       QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_room_timeline_snapshot", [context, handleId, roomId]() {
              return ::komai::rust::matrix_fetch_room_timeline_snapshot(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
        QVector<MatrixTimelineItem> items;
        items.reserve(static_cast<int>(result.size()));
        for (const auto &item : result)
            items.push_back(fromRustTimelineItem(item));
        return items;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::setActiveRoomTimelineInitialPageSize(uint64_t handleId,
                                                                  uint16_t pageSize,
                                                                  QString *errorOut)
{
    try {
        ::komai::rust::matrix_set_active_room_timeline_initial_page_size(handleId, pageSize);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QVector<MatrixTimelineItem>>
MatrixBackendRuntimeService::fetchActiveRoomTimeline(matrix_backend::BlockingCallContext context,
                                                     uint64_t handleId,
                                                     QString *errorOut)
{
    try {
        QElapsedTimer totalTimer;
        totalTimer.start();
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_active_room_timeline", [context, handleId]() {
              return ::komai::rust::matrix_fetch_active_room_timeline(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        const auto rustFetchElapsedUs = totalTimer.nsecsElapsed() / 1000;

        QElapsedTimer convertTimer;
        convertTimer.start();
        QVector<MatrixTimelineItem> items;
        items.reserve(static_cast<int>(result.size()));
        for (const auto &item : result)
            items.push_back(fromRustTimelineItem(item));

        const auto convertElapsedUs = convertTimer.nsecsElapsed() / 1000;

        if (roomSwitchPerfEnabled()) {
            QHash<QString, int> itemKindCounts;
            itemKindCounts.reserve(items.size());
            for (const auto &item : items)
                itemKindCounts[item.itemKind] += 1;

            QStringList itemKindSummary;
            itemKindSummary.reserve(itemKindCounts.size());
            for (auto it = itemKindCounts.cbegin(); it != itemKindCounts.cend(); ++it)
                itemKindSummary.push_back(QStringLiteral("%1:%2").arg(it.key()).arg(it.value()));
            itemKindSummary.sort();

            nhlog::ui()->info(
              "[room-switch-perf] phase=cpp.matrix_backend.fetch_active_room_timeline "
              "handle_id={} item_count={} rust_us={} convert_us={} total_us={} item_kinds={}",
              handleId,
              items.size(),
              rustFetchElapsedUs,
              convertElapsedUs,
              totalTimer.nsecsElapsed() / 1000,
              itemKindSummary.join(QStringLiteral(",")).toStdString());
        }

        return items;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixTimelineItem>>
MatrixBackendRuntimeService::fetchRoomTimeline(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &roomId,
                                               uint16_t limit,
                                               QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_room_timeline", [context, handleId, roomId, limit]() {
              return ::komai::rust::matrix_fetch_room_timeline(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                limit);
          });

        QVector<MatrixTimelineItem> items;
        items.reserve(static_cast<int>(result.size()));
        for (const auto &item : result)
            items.push_back(fromRustTimelineItem(item));
        return items;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::paginateActiveRoomTimelineBackwards(uint64_t handleId,
                                                                 uint16_t pageSize,
                                                                 QString *errorOut)
{
    try {
        ::komai::rust::matrix_paginate_active_room_timeline_backwards(handleId, pageSize);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::sendTypingNotice(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &roomId,
                                              bool typing,
                                              QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_typing_notice",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, typing, context]() {
              ::komai::rust::matrix_send_typing_notice(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                typing);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::sendRoomMessage(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &body,
                                             bool useMarkdownFormatting,
                                             const QString &messageKind,
                                             QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_message",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, body, useMarkdownFormatting, messageKind, context]() {
              ::komai::rust::matrix_send_room_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::sendRoomMessageLikeEventJson(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &roomId,
  const QString &eventType,
  const QString &contentJson,
  QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_message_like_event_json",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventType, contentJson, context]() {
              ::komai::rust::matrix_send_room_message_like_event_json(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventType.toStdString(),
                contentJson.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

void
MatrixBackendRuntimeService::sendCallInvite(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const std::string &callId,
                                            const std::string &partyId,
                                            const std::string &version,
                                            uint32_t lifetime,
                                            const std::string &invitee,
                                            const std::string &offerSdp,
                                            const std::string &offerType)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_invite",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_invite(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
                                                 callId,
                                                 partyId,
                                                 version,
                                                 lifetime,
                                                 invitee,
                                                 offerSdp,
                                                 offerType);
      });
}

void
MatrixBackendRuntimeService::sendCallCandidates(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const std::string &callId,
                                                const std::string &partyId,
                                                const std::string &version,
                                                const komai::voip::CallIceCandidateList &candidates)
{
    // Copy the candidates into a plain std::vector for lambda capture, then build
    // the rust::Vec inside the blocking call where it will be consumed.
    auto capturedCandidates = candidates;
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_candidates",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=, capturedCandidates = std::move(capturedCandidates)]() {
          ::rust::Vec<::komai::rust::MatrixCallIceCandidate> rustCandidates;
          for (const auto &c : capturedCandidates) {
              ::komai::rust::MatrixCallIceCandidate rc;
              rc.sdp_mid          = ::rust::String(c.sdpMid);
              rc.sdp_m_line_index = c.sdpMLineIndex;
              rc.candidate        = ::rust::String(c.candidate);
              rustCandidates.push_back(std::move(rc));
          }
          ::komai::rust::matrix_send_call_candidates(matrix_backend::toRustBlockingContext(context),
                                                     handleId,
                                                     roomId.toStdString(),
                                                     callId,
                                                     partyId,
                                                     version,
                                                     std::move(rustCandidates));
      });
}

void
MatrixBackendRuntimeService::sendCallAnswer(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const std::string &callId,
                                            const std::string &partyId,
                                            const std::string &version,
                                            const std::string &answerSdp,
                                            const std::string &answerType)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_answer",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_answer(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
                                                 callId,
                                                 partyId,
                                                 version,
                                                 answerSdp,
                                                 answerType);
      });
}

void
MatrixBackendRuntimeService::sendCallHangUp(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const std::string &callId,
                                            const std::string &partyId,
                                            const std::string &version,
                                            const std::string &reason)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_hangup",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_hangup(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
                                                 callId,
                                                 partyId,
                                                 version,
                                                 reason);
      });
}

void
MatrixBackendRuntimeService::sendCallSelectAnswer(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &roomId,
                                                  const std::string &callId,
                                                  const std::string &partyId,
                                                  const std::string &version,
                                                  const std::string &selectedPartyId)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_select_answer",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_select_answer(
            matrix_backend::toRustBlockingContext(context),
            handleId,
            roomId.toStdString(),
            callId,
            partyId,
            version,
            selectedPartyId);
      });
}

void
MatrixBackendRuntimeService::sendCallReject(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const std::string &callId,
                                            const std::string &partyId,
                                            const std::string &version)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_reject",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_reject(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
                                                 callId,
                                                 partyId,
                                                 version);
      });
}

void
MatrixBackendRuntimeService::sendCallNegotiate(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &roomId,
                                               const std::string &callId,
                                               const std::string &partyId,
                                               uint32_t lifetime,
                                               const std::string &descSdp,
                                               const std::string &descType)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_negotiate",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_negotiate(matrix_backend::toRustBlockingContext(context),
                                                    handleId,
                                                    roomId.toStdString(),
                                                    callId,
                                                    partyId,
                                                    lifetime,
                                                    descSdp,
                                                    descType);
      });
}

bool
MatrixBackendRuntimeService::sendRoomReplyMessage(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &roomId,
                                                  const QString &repliedToEventId,
                                                  const QString &body,
                                                  bool useMarkdownFormatting,
                                                  const QString &messageKind,
                                                  const QString &threadId,
                                                  QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_reply_message",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId,
           roomId,
           repliedToEventId,
           body,
           useMarkdownFormatting,
           messageKind,
           threadId,
           context]() {
              ::komai::rust::matrix_send_room_reply_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                repliedToEventId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString(),
                threadId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::sendRoomEditMessage(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &targetEventId,
                                                 const QString &body,
                                                 bool useMarkdownFormatting,
                                                 const QString &messageKind,
                                                 QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_edit_message",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, targetEventId, body, useMarkdownFormatting, messageKind, context]() {
              ::komai::rust::matrix_send_room_edit_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                targetEventId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::toggleRoomReaction(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const QString &eventId,
                                                const QString &reactionKey,
                                                QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_toggle_room_reaction",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, reactionKey, context]() {
              ::komai::rust::matrix_toggle_room_reaction(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString(),
                reactionKey.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::redactRoomEvent(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &eventId,
                                             const QString &reason,
                                             QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_redact_room_event",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, reason, context]() {
              ::komai::rust::matrix_redact_room_event(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString(),
                reason.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::markRoomEventAsRead(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &eventId,
                                                 QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_mark_room_event_as_read",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              ::komai::rust::matrix_mark_room_event_as_read(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::reportRoomEvent(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &eventId,
                                             const QString &reason,
                                             int score,
                                             QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_report_room_event",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, reason, score, context]() {
              ::komai::rust::matrix_report_room_event(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString(),
                reason.toStdString(),
                score);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QStringList>
MatrixBackendRuntimeService::fetchRoomPinnedEventIds(matrix_backend::BlockingCallContext context,
                                                     uint64_t handleId,
                                                     const QString &roomId,
                                                     QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_room_pinned_event_ids",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, context]() {
              return ::komai::rust::matrix_fetch_room_pinned_event_ids(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
        QStringList pinnedEventIds;
        pinnedEventIds.reserve(static_cast<qsizetype>(result.size()));
        for (const auto &eventId : result)
            pinnedEventIds.push_back(QString::fromStdString(std::string(eventId)));
        return pinnedEventIds;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QStringList>
MatrixBackendRuntimeService::fetchRoomFrequentReactions(matrix_backend::BlockingCallContext context,
                                                        uint64_t handleId,
                                                        const QString &roomId,
                                                        int lookbackDays,
                                                        int maxResults,
                                                        uint64_t maxScannedEvents,
                                                        QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_room_frequent_reactions",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, lookbackDays, maxResults, maxScannedEvents, context]() {
              return ::komai::rust::matrix_fetch_room_frequent_reactions(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                lookbackDays,
                maxResults > 0 ? static_cast<uint32_t>(maxResults) : 0,
                maxScannedEvents);
          });
        QStringList frequentReactions;
        frequentReactions.reserve(static_cast<qsizetype>(result.size()));
        for (const auto &reaction : result)
            frequentReactions.push_back(QString::fromStdString(std::string(reaction)));
        return frequentReactions;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::pinRoomEvent(matrix_backend::BlockingCallContext context,
                                          uint64_t handleId,
                                          const QString &roomId,
                                          const QString &eventId,
                                          QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_pin_room_event",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              ::komai::rust::matrix_pin_room_event(matrix_backend::toRustBlockingContext(context),
                                                   handleId,
                                                   roomId.toStdString(),
                                                   eventId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unpinRoomEvent(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const QString &eventId,
                                            QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_unpin_room_event",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              ::komai::rust::matrix_unpin_room_event(matrix_backend::toRustBlockingContext(context),
                                                     handleId,
                                                     roomId.toStdString(),
                                                     eventId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixBackendRuntimeService::RawEventDialogData>
MatrixBackendRuntimeService::fetchActiveRoomRawEventDialogData(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &roomId,
  const QString &eventId,
  QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_active_room_raw_event_dialog_data",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              return ::komai::rust::matrix_fetch_active_room_raw_event_dialog_data(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString());
          });
        return RawEventDialogData{
          .prettyJson    = QString::fromStdString(std::string(result.pretty_json)),
          .body          = QString::fromStdString(std::string(result.body)),
          .formattedBody = QString::fromStdString(std::string(result.formatted_body)),
        };
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixBackendRuntimeService::EventContentForForwarding>
MatrixBackendRuntimeService::fetchActiveRoomEventContentForForwarding(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &roomId,
  const QString &eventId,
  QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_active_room_event_content_for_forwarding",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              return ::komai::rust::matrix_fetch_active_room_event_content_for_forwarding(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString());
          });
        return EventContentForForwarding{
          .eventType   = QString::fromStdString(std::string(result.event_type)),
          .contentJson = QString::fromStdString(std::string(result.content_json)),
        };
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixReadReceiptEntry>>
MatrixBackendRuntimeService::fetchRoomReadReceipts(matrix_backend::BlockingCallContext context,
                                                   uint64_t handleId,
                                                   const QString &roomId,
                                                   const QString &eventId,
                                                   QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_room_read_receipts",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              return ::komai::rust::matrix_fetch_room_read_receipts(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString());
          });
        QVector<MatrixReadReceiptEntry> receipts;
        receipts.reserve(static_cast<int>(result.size()));
        for (const auto &entry : result)
            receipts.push_back(fromRustReadReceiptEntry(entry));
        return receipts;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::sendRoomAttachment(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const QString &filePath,
                                                const QString &filename,
                                                const QString &caption,
                                                const QString &replyEventId,
                                                const QString &threadId,
                                                const QString &mimeType,
                                                uint64_t durationMs,
                                                bool isVoice,
                                                const QList<float> &waveform,
                                                QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_attachment",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId,
           roomId,
           filePath,
           filename,
           caption,
           replyEventId,
           threadId,
           mimeType,
           durationMs,
           isVoice,
           waveform,
           context]() {
              const auto waveformSlice = ::rust::Slice<const float>(
                waveform.constData(), static_cast<size_t>(waveform.size()));
              ::komai::rust::matrix_send_room_attachment(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                filePath.toStdString(),
                filename.toStdString(),
                caption.toStdString(),
                replyEventId.toStdString(),
                threadId.toStdString(),
                mimeType.toStdString(),
                durationMs,
                isVoice,
                waveformSlice);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::uploadMedia(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &filePath,
                                         const QString &mimeType,
                                         QString *errorOut)
{
    try {
        return matrix::normalizeMxcUri(QString::fromStdString(std::string(
          invokeRuntimeWorkerCall("matrix_upload_media", [context, handleId, filePath, mimeType]() {
              return ::komai::rust::matrix_upload_media(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                filePath.toStdString(),
                mimeType.toStdString());
          }))));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::sendRoomImage(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           const QString &roomId,
                                           const QString &mxcUri,
                                           const QString &body,
                                           const QString &filename,
                                           const QString &infoJson,
                                           QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_send_room_image",
                                [context, handleId, roomId, mxcUri, body, filename, infoJson]() {
                                    ::komai::rust::matrix_send_room_image(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      mxcUri.toStdString(),
                                      body.toStdString(),
                                      filename.toStdString(),
                                      infoJson.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QByteArray>
MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &itemId,
  int width,
  int height,
  bool crop,
  QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_active_room_timeline_media_content",
          [context, handleId, &itemId, width, height, crop]() {
              return ::komai::rust::matrix_fetch_active_room_timeline_media_content(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                itemId.toStdString(),
                width,
                height,
                crop);
          });
        QByteArray data;
        data.reserve(static_cast<qsizetype>(result.size()));
        data.append(reinterpret_cast<const char *>(result.data()),
                    static_cast<qsizetype>(result.size()));
        return data;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QByteArray>
MatrixBackendRuntimeService::fetchMediaContent(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &mxcUri,
                                               int width,
                                               int height,
                                               bool crop,
                                               QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_media_content", [context, handleId, &mxcUri, width, height, crop]() {
              return ::komai::rust::matrix_fetch_media_content(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                mxcUri.toStdString(),
                width,
                height,
                crop);
          });
        QByteArray data;
        data.reserve(static_cast<qsizetype>(result.size()));
        data.append(reinterpret_cast<const char *>(result.data()),
                    static_cast<qsizetype>(result.size()));
        return data;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

} // namespace komai
