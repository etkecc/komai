// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "events/EventAccessors.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <type_traits>

#include <mtx/events/collections.hpp>

namespace {

struct IsStateEvent
{
    template<class T>
    constexpr bool operator()(const mtx::events::StateEvent<T> &)
    {
        return true;
    }
    template<class T>
    constexpr bool operator()(const mtx::events::Event<T> &)
    {
        return false;
    }
};

struct EventMsgType
{
    template<class T>
    mtx::events::MessageType operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires(decltype(e) t) { t.content.msgtype.value(); })
            return mtx::events::getMessageType(e.content.msgtype.value());
        else if constexpr (requires(decltype(e) t) { std::string{t.content.msgtype}; })
            return mtx::events::getMessageType(e.content.msgtype);
        return mtx::events::MessageType::Unknown;
    }
};

struct EventType
{
    template<class T>
    mtx::events::EventType operator()(const mtx::events::Event<T> &e)
    {
        return e.type;
    }
};

struct CallType
{
    template<class T>
    std::string operator()(const T &e)
    {
        if constexpr (std::is_same_v<mtx::events::RoomEvent<mtx::events::voip::CallInvite>, T>) {
            const char video[]     = "m=video";
            const std::string &sdp = e.content.offer.sdp;
            return std::search(sdp.cbegin(),
                               sdp.cend(),
                               std::cbegin(video),
                               std::cend(video) - 1,
                               [](unsigned char c1, unsigned char c2) {
                                   return std::tolower(c1) == std::tolower(c2);
                               }) != sdp.cend()
                     ? "video"
                     : "voice";
        }
        return "";
    }
};

struct EventRelations
{
    inline const static mtx::common::Relations empty;

    template<class T>
    const mtx::common::Relations &operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::relations; }) {
            return e.content.relations;
        }
        return empty;
    }
};

struct EventMentions
{
    template<class T>
    std::optional<mtx::common::Mentions> operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::mentions; }) {
            return e.content.mentions;
        }
        return std::nullopt;
    }
};

struct SetEventRelations
{
    mtx::common::Relations new_relations;
    template<class T>
    void operator()(mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::relations; }) {
            e.content.relations = std::move(new_relations);
        }
    }
};

struct EventTransactionId
{
    template<class T>
    std::string operator()(const mtx::events::RoomEvent<T> &e)
    {
        return e.unsigned_data.transaction_id;
    }
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        return e.unsigned_data.transaction_id;
    }
};
}

const std::string &
mtx::accessors::event_id(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) -> const std::string & { return e.event_id; }, event);
}
const std::string &
mtx::accessors::room_id(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) -> const std::string & { return e.room_id; }, event);
}

const std::string &
mtx::accessors::sender(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) -> const std::string & { return e.sender; }, event);
}

QDateTime
mtx::accessors::origin_server_ts(const mtx::events::collections::TimelineEvents &event)
{
    return QDateTime::fromMSecsSinceEpoch(origin_server_ts_ms(event));
}

std::uint64_t
mtx::accessors::origin_server_ts_ms(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) { return e.origin_server_ts; }, event);
}

mtx::events::EventType
mtx::accessors::event_type(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventType{}, event);
}
mtx::events::MessageType
mtx::accessors::msg_type(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMsgType{}, event);
}

std::string
mtx::accessors::room_name(const mtx::events::collections::TimelineEvents &event)
{
    if (auto c = std::get_if<mtx::events::StateEvent<mtx::events::state::Name>>(&event))
        return c->content.name;
    else
        return "";
}

std::string
mtx::accessors::room_topic(const mtx::events::collections::TimelineEvents &event)
{
    if (auto c = std::get_if<mtx::events::StateEvent<mtx::events::state::Topic>>(&event))
        return c->content.topic;
    else
        return "";
}

std::string
mtx::accessors::call_type(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(CallType{}, event);
}
const mtx::common::Relations &
mtx::accessors::relations(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventRelations{}, event);
}
std::optional<mtx::common::Mentions>
mtx::accessors::mentions(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMentions{}, event);
}

void
mtx::accessors::set_relations(mtx::events::collections::TimelineEvents &event,
                              mtx::common::Relations relations)
{
    std::visit(SetEventRelations{std::move(relations)}, event);
}

std::string
mtx::accessors::transaction_id(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventTransactionId{}, event);
}

nlohmann::json
mtx::accessors::serialize_event(const mtx::events::collections::TimelineEvents &event)
{
    nlohmann::json serialized    = nlohmann::json::object();
    serialized["event_id"]       = event_id(event);
    serialized["room_id"]        = room_id(event);
    serialized["sender"]         = sender(event);
    serialized["timestamp"]      = origin_server_ts_ms(event);
    serialized["body"]           = body(event);
    serialized["formatted_body"] = formatted_body(event);
    serialized["url"]            = url(event);
    serialized["thumbnail_url"]  = thumbnail_url(event);
    serialized["is_state_event"] = is_state_event(event);
    return serialized;
}

bool
mtx::accessors::is_state_event(const mtx::events::collections::StateEvents &event)
{
    return std::visit(IsStateEvent{}, event);
}

bool
mtx::accessors::is_state_event(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(IsStateEvent{}, event);
}

template<typename T>
static constexpr auto
isMessage(const mtx::events::RoomEvent<T> &e)
  -> std::enable_if_t<std::is_same<decltype(e.content.msgtype), std::string>::value, bool>
{
    return true;
}

template<typename T>
static constexpr auto
isMessage(const mtx::events::Event<T> &)
{
    return false;
}

template<typename T>
static constexpr auto
isMessage(const mtx::events::EncryptedEvent<T> &)
{
    return true;
}

static constexpr auto
isMessage(const mtx::events::RoomEvent<mtx::events::voip::CallInvite> &)
{
    return true;
}

static constexpr auto
isMessage(const mtx::events::RoomEvent<mtx::events::voip::CallAnswer> &)
{
    return true;
}
static constexpr auto
isMessage(const mtx::events::RoomEvent<mtx::events::voip::CallHangUp> &)
{
    return true;
}

static constexpr auto
isMessage(const mtx::events::RoomEvent<mtx::events::voip::CallReject> &)
{
    return true;
}
static constexpr auto
isMessage(const mtx::events::RoomEvent<mtx::events::voip::CallSelectAnswer> &)
{
    return true;
}

bool
mtx::accessors::is_message(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) { return isMessage(e); }, event);
}
