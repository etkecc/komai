// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>

#include "mtx/events/collections.hpp"
#include "mtx/events/voip.hpp"

namespace komai::voip {

struct MatrixCallEventPayload
{
    QString eventType;
    QString contentJson;
};

inline void
addMatrixCallVersion(nlohmann::json &json, std::string_view version)
{
    if (version == "0")
        json["version"] = 0;
    else
        json["version"] = version;
}

namespace detail {

inline std::string
requiredJsonString(const nlohmann::json &json, std::string_view key)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_string())
        throw std::runtime_error(std::string("missing or invalid string field '") +
                                 std::string(key) + "'");
    return it->get<std::string>();
}

inline std::string
optionalJsonString(const nlohmann::json &json, std::string_view key)
{
    const auto it = json.find(key);
    if (it == json.end() || it->is_null())
        return {};
    if (!it->is_string())
        throw std::runtime_error(std::string("invalid string field '") + std::string(key) + "'");
    return it->get<std::string>();
}

inline uint32_t
optionalJsonUint32(const nlohmann::json &json, std::string_view key, uint32_t defaultValue)
{
    const auto it = json.find(key);
    if (it == json.end() || it->is_null())
        return defaultValue;
    if (!it->is_number_unsigned() && !it->is_number_integer())
        throw std::runtime_error(std::string("invalid integer field '") + std::string(key) + "'");
    return it->get<uint32_t>();
}

inline std::string
parseMatrixCallVersion(const nlohmann::json &json, std::string_view defaultVersion)
{
    const auto it = json.find("version");
    if (it == json.end() || it->is_null())
        return std::string(defaultVersion);
    if (it->is_string())
        return it->get<std::string>();
    if (it->is_number_unsigned() || it->is_number_integer())
        return std::to_string(it->get<int>());

    throw std::runtime_error("invalid version field");
}

inline mtx::events::voip::RTCSessionDescriptionInit
parseSessionDescription(const nlohmann::json &json, std::string_view key)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_object())
        throw std::runtime_error(std::string("missing or invalid object field '") +
                                 std::string(key) + "'");

    const auto &object = *it;
    const auto type    = requiredJsonString(object, "type");
    return mtx::events::voip::RTCSessionDescriptionInit{
      .sdp  = requiredJsonString(object, "sdp"),
      .type = type == "answer" ? mtx::events::voip::RTCSessionDescriptionInit::Type::Answer
                               : mtx::events::voip::RTCSessionDescriptionInit::Type::Offer,
    };
}

inline std::vector<mtx::events::voip::CallCandidates::Candidate>
parseCandidates(const nlohmann::json &json)
{
    const auto it = json.find("candidates");
    if (it == json.end() || !it->is_array())
        throw std::runtime_error("missing or invalid array field 'candidates'");

    std::vector<mtx::events::voip::CallCandidates::Candidate> candidates;
    candidates.reserve(it->size());
    for (const auto &entry : *it) {
        if (!entry.is_object())
            throw std::runtime_error("invalid candidate entry");

        candidates.push_back(mtx::events::voip::CallCandidates::Candidate{
          .sdpMid        = requiredJsonString(entry, "sdpMid"),
          .sdpMLineIndex = static_cast<uint16_t>(optionalJsonUint32(entry, "sdpMLineIndex", 0)),
          .candidate     = requiredJsonString(entry, "candidate"),
        });
    }

    return candidates;
}

inline mtx::events::voip::CallHangUp::Reason
parseHangupReason(const nlohmann::json &json)
{
    const auto reason = optionalJsonString(json, "reason");
    if (reason == "ice_failed")
        return mtx::events::voip::CallHangUp::Reason::ICEFailed;
    if (reason == "invite_timeout")
        return mtx::events::voip::CallHangUp::Reason::InviteTimeOut;
    if (reason == "ice_timeout")
        return mtx::events::voip::CallHangUp::Reason::ICETimeOut;
    if (reason == "user_media_failed")
        return mtx::events::voip::CallHangUp::Reason::UserMediaFailed;
    if (reason == "user_busy")
        return mtx::events::voip::CallHangUp::Reason::UserBusy;
    if (reason == "unknown_error")
        return mtx::events::voip::CallHangUp::Reason::UnknownError;
    if (reason == "user")
        return mtx::events::voip::CallHangUp::Reason::User;
    return mtx::events::voip::CallHangUp::Reason::UserHangUp;
}

inline nlohmann::json
parseMatrixCallJson(const QString &contentJson)
{
    return nlohmann::json::parse(contentJson.toStdString());
}

} // namespace detail

inline bool
parseMatrixCallEventJson(const QString &contentJson,
                         mtx::events::voip::CallInvite &content,
                         QString *error = nullptr)
{
    try {
        const auto json  = detail::parseMatrixCallJson(contentJson);
        content.call_id  = detail::requiredJsonString(json, "call_id");
        content.party_id = detail::optionalJsonString(json, "party_id");
        content.offer    = detail::parseSessionDescription(json, "offer");
        content.version  = detail::parseMatrixCallVersion(json, "0");
        content.lifetime = detail::optionalJsonUint32(json, "lifetime", 90000);
        content.invitee  = detail::optionalJsonString(json, "invitee");
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

inline bool
parseMatrixCallEventJson(const QString &contentJson,
                         mtx::events::voip::CallCandidates &content,
                         QString *error = nullptr)
{
    try {
        const auto json    = detail::parseMatrixCallJson(contentJson);
        content.call_id    = detail::requiredJsonString(json, "call_id");
        content.party_id   = detail::optionalJsonString(json, "party_id");
        content.candidates = detail::parseCandidates(json);
        content.version    = detail::parseMatrixCallVersion(json, "0");
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

inline bool
parseMatrixCallEventJson(const QString &contentJson,
                         mtx::events::voip::CallAnswer &content,
                         QString *error = nullptr)
{
    try {
        const auto json  = detail::parseMatrixCallJson(contentJson);
        content.call_id  = detail::requiredJsonString(json, "call_id");
        content.party_id = detail::optionalJsonString(json, "party_id");
        content.version  = detail::parseMatrixCallVersion(json, "0");
        content.answer   = detail::parseSessionDescription(json, "answer");
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

inline bool
parseMatrixCallEventJson(const QString &contentJson,
                         mtx::events::voip::CallHangUp &content,
                         QString *error = nullptr)
{
    try {
        const auto json  = detail::parseMatrixCallJson(contentJson);
        content.call_id  = detail::requiredJsonString(json, "call_id");
        content.party_id = detail::optionalJsonString(json, "party_id");
        content.version  = detail::parseMatrixCallVersion(json, "0");
        content.reason   = detail::parseHangupReason(json);
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

inline bool
parseMatrixCallEventJson(const QString &contentJson,
                         mtx::events::voip::CallSelectAnswer &content,
                         QString *error = nullptr)
{
    try {
        const auto json           = detail::parseMatrixCallJson(contentJson);
        content.call_id           = detail::requiredJsonString(json, "call_id");
        content.party_id          = detail::requiredJsonString(json, "party_id");
        content.version           = detail::parseMatrixCallVersion(json, "1");
        content.selected_party_id = detail::requiredJsonString(json, "selected_party_id");
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

inline bool
parseMatrixCallEventJson(const QString &contentJson,
                         mtx::events::voip::CallReject &content,
                         QString *error = nullptr)
{
    try {
        const auto json  = detail::parseMatrixCallJson(contentJson);
        content.call_id  = detail::requiredJsonString(json, "call_id");
        content.party_id = detail::requiredJsonString(json, "party_id");
        content.version  = detail::parseMatrixCallVersion(json, "1");
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

inline bool
parseMatrixCallEventJson(const QString &contentJson,
                         mtx::events::voip::CallNegotiate &content,
                         QString *error = nullptr)
{
    try {
        const auto json     = detail::parseMatrixCallJson(contentJson);
        content.call_id     = detail::requiredJsonString(json, "call_id");
        content.party_id    = detail::requiredJsonString(json, "party_id");
        content.lifetime    = detail::optionalJsonUint32(json, "lifetime", 90000);
        content.description = detail::parseSessionDescription(json, "description");
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

template<typename EventContent>
inline mtx::events::RoomEvent<EventContent>
makeMatrixCallRoomEvent(const QString &roomId,
                        const QString &senderId,
                        const QString &eventId,
                        EventContent content)
{
    mtx::events::RoomEvent<EventContent> event;
    event.type             = mtx::events::message_content_to_type<EventContent>;
    event.sender           = senderId.toStdString();
    event.content          = std::move(content);
    event.event_id         = eventId.toStdString();
    event.room_id          = roomId.toStdString();
    event.origin_server_ts = 0;
    return event;
}

inline nlohmann::json
serializeMatrixCallEventJsonObject(const mtx::events::voip::RTCSessionDescriptionInit &content)
{
    nlohmann::json json;
    json["sdp"]  = content.sdp;
    json["type"] = content.type == mtx::events::voip::RTCSessionDescriptionInit::Type::Answer
                     ? "answer"
                     : "offer";
    return json;
}

inline nlohmann::json
serializeMatrixCallEventJsonObject(const mtx::events::voip::CallCandidates::Candidate &content)
{
    nlohmann::json json;
    json["sdpMid"]        = content.sdpMid;
    json["sdpMLineIndex"] = content.sdpMLineIndex;
    json["candidate"]     = content.candidate;
    return json;
}

inline MatrixCallEventPayload
toMatrixCallEventPayload(const mtx::events::voip::CallInvite &content)
{
    nlohmann::json json;
    json["call_id"]  = content.call_id;
    json["offer"]    = serializeMatrixCallEventJsonObject(content.offer);
    json["lifetime"] = content.lifetime;
    addMatrixCallVersion(json, content.version);
    if (content.version != "0") {
        json["party_id"] = content.party_id;
        if (!content.invitee.empty())
            json["invitee"] = content.invitee;
    }

    return {
      .eventType   = QStringLiteral("m.call.invite"),
      .contentJson = QString::fromStdString(json.dump()),
    };
}

inline MatrixCallEventPayload
toMatrixCallEventPayload(const mtx::events::voip::CallCandidates &content)
{
    nlohmann::json json;
    json["call_id"]    = content.call_id;
    json["candidates"] = nlohmann::json::array();
    for (const auto &candidate : content.candidates)
        json["candidates"].push_back(serializeMatrixCallEventJsonObject(candidate));
    addMatrixCallVersion(json, content.version);
    if (content.version != "0")
        json["party_id"] = content.party_id;

    return {
      .eventType   = QStringLiteral("m.call.candidates"),
      .contentJson = QString::fromStdString(json.dump()),
    };
}

inline MatrixCallEventPayload
toMatrixCallEventPayload(const mtx::events::voip::CallAnswer &content)
{
    nlohmann::json json;
    json["call_id"] = content.call_id;
    json["answer"]  = serializeMatrixCallEventJsonObject(content.answer);
    addMatrixCallVersion(json, content.version);
    if (content.version != "0")
        json["party_id"] = content.party_id;

    return {
      .eventType   = QStringLiteral("m.call.answer"),
      .contentJson = QString::fromStdString(json.dump()),
    };
}

inline MatrixCallEventPayload
toMatrixCallEventPayload(const mtx::events::voip::CallHangUp &content)
{
    nlohmann::json json;
    json["call_id"] = content.call_id;
    addMatrixCallVersion(json, content.version);
    if (content.version != "0")
        json["party_id"] = content.party_id;

    switch (content.reason) {
    case mtx::events::voip::CallHangUp::Reason::ICEFailed:
        json["reason"] = "ice_failed";
        break;
    case mtx::events::voip::CallHangUp::Reason::InviteTimeOut:
        json["reason"] = "invite_timeout";
        break;
    case mtx::events::voip::CallHangUp::Reason::ICETimeOut:
        json["reason"] = "ice_timeout";
        break;
    case mtx::events::voip::CallHangUp::Reason::UserHangUp:
        json["reason"] = "user_hangup";
        break;
    case mtx::events::voip::CallHangUp::Reason::UserMediaFailed:
        json["reason"] = "user_media_failed";
        break;
    case mtx::events::voip::CallHangUp::Reason::UserBusy:
        json["reason"] = "user_busy";
        break;
    case mtx::events::voip::CallHangUp::Reason::UnknownError:
        json["reason"] = "unknown_error";
        break;
    case mtx::events::voip::CallHangUp::Reason::User:
        break;
    }

    return {
      .eventType   = QStringLiteral("m.call.hangup"),
      .contentJson = QString::fromStdString(json.dump()),
    };
}

inline MatrixCallEventPayload
toMatrixCallEventPayload(const mtx::events::voip::CallSelectAnswer &content)
{
    nlohmann::json json;
    json["call_id"]           = content.call_id;
    json["party_id"]          = content.party_id;
    json["selected_party_id"] = content.selected_party_id;
    addMatrixCallVersion(json, content.version);

    return {
      .eventType   = QStringLiteral("m.call.select_answer"),
      .contentJson = QString::fromStdString(json.dump()),
    };
}

inline MatrixCallEventPayload
toMatrixCallEventPayload(const mtx::events::voip::CallReject &content)
{
    nlohmann::json json;
    json["call_id"]  = content.call_id;
    json["party_id"] = content.party_id;
    addMatrixCallVersion(json, content.version);

    return {
      .eventType   = QStringLiteral("m.call.reject"),
      .contentJson = QString::fromStdString(json.dump()),
    };
}

inline MatrixCallEventPayload
toMatrixCallEventPayload(const mtx::events::voip::CallNegotiate &content)
{
    nlohmann::json json;
    json["call_id"]     = content.call_id;
    json["party_id"]    = content.party_id;
    json["lifetime"]    = content.lifetime;
    json["description"] = serializeMatrixCallEventJsonObject(content.description);
    json["version"]     = "1";

    return {
      .eventType   = QStringLiteral("m.call.negotiate"),
      .contentJson = QString::fromStdString(json.dump()),
    };
}

} // namespace komai::voip
