// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineMessageSendPipeline.h"

#include <QDateTime>

#include <nlohmann/json.hpp>

#include <string>
#include <type_traits>

#include "Logging.h"
#include "MatrixClient.h"
#include "cache/Cache.h"
#include "encryption/Olm.h"
#include "events/EventAccessors.h"
#include "utils/Utils.h"

namespace timeline::send {
template<typename T>
void
sendEncryptedMessage(const QString &roomId,
                     const mtx::events::RoomEvent<T> &msg,
                     mtx::events::EventType eventType,
                     const AddPendingMessageFn &addPendingMessage,
                     const NotifyEncryptionFailureFn &notifyEncryptionFailure)
{
    const auto roomIdStd = roomId.toStdString();

    using namespace mtx::events;
    nlohmann::json doc = {{"type", mtx::events::to_string(eventType)},
                          {"content", nlohmann::json(msg.content)},
                          {"room_id", roomIdStd}};

    try {
        mtx::events::EncryptedEvent<mtx::events::msg::Encrypted> event;
        event.content  = olm::encrypt_group_message(roomIdStd, http::client()->device_id(), doc);
        event.event_id = msg.event_id;
        event.room_id  = roomIdStd;
        event.sender   = utils::localUser().toStdString();
        event.type     = mtx::events::EventType::RoomEncrypted;
        event.origin_server_ts = QDateTime::currentMSecsSinceEpoch();
        addPendingMessage(event);
    } catch (const mtx::crypto::olm_exception &e) {
        nhlog::crypto()->critical(
          "failed to open outbound megolm session ({}): {}", roomIdStd, e.what());
        notifyEncryptionFailure();
    } catch (const std::exception &e) {
        nhlog::db()->critical(
          "failed to open outbound megolm session ({}): {}", roomIdStd, e.what());
        notifyEncryptionFailure();
    }
}

struct SendMessageVisitor
{
    SendMessageVisitor(const QString &roomId,
                       const AddPendingMessageFn &addPendingMessage,
                       const EmitEncryptedImageFn &emitEncryptedImage,
                       const NotifyEncryptionFailureFn &notifyEncryptionFailure)
      : roomId_(roomId)
      , addPendingMessage_(addPendingMessage)
      , emitEncryptedImage_(emitEncryptedImage)
      , notifyEncryptionFailure_(notifyEncryptionFailure)
    {
    }

    template<typename T, mtx::events::EventType Event>
    void sendRoomEvent(mtx::events::RoomEvent<T> msg)
    {
        if (cache::isRoomEncrypted(roomId_.toStdString())) {
            if (const auto encInfo = mtx::accessors::file(msg); encInfo)
                emitEncryptedImage_(encInfo.value());

            if (const auto thumbInfo = mtx::accessors::thumbnail_file(msg); thumbInfo)
                emitEncryptedImage_(thumbInfo.value());

            sendEncryptedMessage(roomId_, msg, Event, addPendingMessage_, notifyEncryptionFailure_);
            return;
        }

        msg.type = Event;
        addPendingMessage_(msg);
    }

    // Do-nothing operator for all unhandled events
    template<typename T>
    void operator()(const mtx::events::Event<T> &)
    {
    }

    // Operator for m.room.message events that contain a msgtype in their content.
    template<typename T,
             std::enable_if_t<std::is_same<decltype(T::msgtype), std::string>::value, int> = 0>
    void operator()(mtx::events::RoomEvent<T> msg)
    {
        sendRoomEvent<T, mtx::events::EventType::RoomMessage>(msg);
    }

    // Reactions must keep relation data outside ciphertext for homeserver compatibility.
    void operator()(mtx::events::RoomEvent<mtx::events::msg::Reaction> msg)
    {
        msg.type = mtx::events::EventType::Reaction;
        addPendingMessage_(msg);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::voip::CallInvite> &event)
    {
        sendRoomEvent<mtx::events::voip::CallInvite, mtx::events::EventType::CallInvite>(event);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::voip::CallCandidates> &event)
    {
        sendRoomEvent<mtx::events::voip::CallCandidates, mtx::events::EventType::CallCandidates>(
          event);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::voip::CallAnswer> &event)
    {
        sendRoomEvent<mtx::events::voip::CallAnswer, mtx::events::EventType::CallAnswer>(event);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::voip::CallHangUp> &event)
    {
        sendRoomEvent<mtx::events::voip::CallHangUp, mtx::events::EventType::CallHangUp>(event);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::voip::CallSelectAnswer> &event)
    {
        sendRoomEvent<mtx::events::voip::CallSelectAnswer,
                      mtx::events::EventType::CallSelectAnswer>(event);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::voip::CallReject> &event)
    {
        sendRoomEvent<mtx::events::voip::CallReject, mtx::events::EventType::CallReject>(event);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::voip::CallNegotiate> &event)
    {
        sendRoomEvent<mtx::events::voip::CallNegotiate, mtx::events::EventType::CallNegotiate>(
          event);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationRequest> &msg)
    {
        sendRoomEvent<mtx::events::msg::KeyVerificationRequest,
                      mtx::events::EventType::RoomMessage>(msg);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationReady> &msg)
    {
        sendRoomEvent<mtx::events::msg::KeyVerificationReady,
                      mtx::events::EventType::KeyVerificationReady>(msg);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationStart> &msg)
    {
        sendRoomEvent<mtx::events::msg::KeyVerificationStart,
                      mtx::events::EventType::KeyVerificationStart>(msg);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationAccept> &msg)
    {
        sendRoomEvent<mtx::events::msg::KeyVerificationAccept,
                      mtx::events::EventType::KeyVerificationAccept>(msg);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationMac> &msg)
    {
        sendRoomEvent<mtx::events::msg::KeyVerificationMac,
                      mtx::events::EventType::KeyVerificationMac>(msg);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationKey> &msg)
    {
        sendRoomEvent<mtx::events::msg::KeyVerificationKey,
                      mtx::events::EventType::KeyVerificationKey>(msg);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationDone> &msg)
    {
        sendRoomEvent<mtx::events::msg::KeyVerificationDone,
                      mtx::events::EventType::KeyVerificationDone>(msg);
    }

    void operator()(const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationCancel> &msg)
    {
        sendRoomEvent<mtx::events::msg::KeyVerificationCancel,
                      mtx::events::EventType::KeyVerificationCancel>(msg);
    }

    void operator()(mtx::events::Sticker msg)
    {
        msg.type = mtx::events::EventType::Sticker;
        if (cache::isRoomEncrypted(roomId_.toStdString())) {
            sendEncryptedMessage(roomId_,
                                 msg,
                                 mtx::events::EventType::Sticker,
                                 addPendingMessage_,
                                 notifyEncryptionFailure_);
        } else {
            addPendingMessage_(msg);
        }
    }

    const QString &roomId_;
    const AddPendingMessageFn &addPendingMessage_;
    const EmitEncryptedImageFn &emitEncryptedImage_;
    const NotifyEncryptionFailureFn &notifyEncryptionFailure_;
};
}

void
timeline::send::sendPendingMessage(const QString &roomId,
                                   mtx::events::collections::TimelineEvents message,
                                   const AddPendingMessageFn &addPendingMessage,
                                   const EmitEncryptedImageFn &emitEncryptedImage,
                                   const NotifyEncryptionFailureFn &notifyEncryptionFailure)
{
    std::visit(
      [](auto &msg) {
          // Gets overwritten for reactions and stickers in SendMessageVisitor.
          msg.type             = mtx::events::EventType::RoomMessage;
          msg.event_id         = "m" + http::client()->generate_txn_id();
          msg.sender           = utils::localUser().toStdString();
          msg.origin_server_ts = QDateTime::currentMSecsSinceEpoch();
      },
      message);

    std::visit(
      SendMessageVisitor{roomId, addPendingMessage, emitEncryptedImage, notifyEncryptionFailure},
      message);
}
