// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "matrix/backend/MatrixBlockingCall.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "voip/CallTypes.h"

namespace komai {

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

std::optional<QString>
MatrixBackendRuntimeService::sendRoomMessage(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &body,
                                             bool useMarkdownFormatting,
                                             const QString &messageKind,
                                             MatrixSendMode sendMode,
                                             const QString &mentionUserIds,
                                             bool mentionsRoom,
                                             QString *errorOut)
{
    try {
        ::rust::String eventId;
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_message",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId,
           roomId,
           body,
           useMarkdownFormatting,
           messageKind,
           mentionUserIds,
           mentionsRoom,
           sendMode,
           context,
           &eventId]() {
              eventId = ::komai::rust::matrix_send_room_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString(),
                mentionUserIds.toStdString(),
                mentionsRoom,
                sendMode == MatrixSendMode::Queued);
          });
        return QString::fromStdString(std::string(eventId));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
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
                                                  const QString &mentionUserIds,
                                                  bool mentionsRoom,
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
           mentionUserIds,
           mentionsRoom,
           context]() {
              ::komai::rust::matrix_send_room_reply_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                repliedToEventId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString(),
                threadId.toStdString(),
                mentionUserIds.toStdString(),
                mentionsRoom);
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
                                                 const QString &mentionUserIds,
                                                 bool mentionsRoom,
                                                 QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_edit_message",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId,
           roomId,
           targetEventId,
           body,
           useMarkdownFormatting,
           messageKind,
           mentionUserIds,
           mentionsRoom,
           context]() {
              ::komai::rust::matrix_send_room_edit_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                targetEventId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString(),
                mentionUserIds.toStdString(),
                mentionsRoom);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

} // namespace komai
