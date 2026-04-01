// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <nlohmann/json.hpp>
#include <string_view>

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
