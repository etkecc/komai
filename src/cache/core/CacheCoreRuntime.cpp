// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "Logging.h"
#include "encryption/Olm.h"
#include "events/EventAccessors.h"

namespace {
using HiddenEventsContent = mtx::events::account_data::nheko_extensions::HiddenEvents;
constexpr std::string_view KOMAI_HIDDEN_EVENTS_TYPE = "cc.etke.komai.hidden_events";

std::optional<HiddenEventsContent>
parseHiddenEventsFromRawAccountData(const std::string &eventJson)
{
    try {
        const auto parsedEvent = nlohmann::json::parse(eventJson);
        if (!parsedEvent.is_object() || !parsedEvent.contains("content"))
            return std::nullopt;

        auto content = parsedEvent.at("content").get<HiddenEventsContent>();
        if (content.hidden_event_types)
            return content;
    } catch (const std::exception &) {
    }

    return std::nullopt;
}
} // namespace

bool
MatrixStore::isHiddenEvent(db::Transaction &txn,
                           mtx::events::collections::TimelineEvents e,
                           const std::string &room_id)
{
    using namespace mtx::events;

    // Always hide edits
    if (mtx::accessors::relations(e).replaces())
        return true;

    if (auto encryptedEvent = std::get_if<EncryptedEvent<msg::Encrypted>>(&e)) {
        MegolmSessionIndex index;
        index.room_id    = room_id;
        index.session_id = encryptedEvent->content.session_id;

        auto result = olm::decryptEvent(index, *encryptedEvent, true);
        if (!result.error)
            e = result.event.value();
    }

    HiddenEventsContent hiddenEvents;
    hiddenEvents.hidden_event_types = std::vector{
      EventType::Reaction,
      EventType::CallCandidates,
      EventType::CallNegotiate,
      EventType::Unsupported,
    };
    // check if selected answer is from to local user
    /*
     * localUser accepts/rejects the call and it is selected by caller - No message
     * Another User accepts/rejects the call and it is selected by caller - "Call answered/rejected
     * elsewhere"
     */
    bool callLocalUser_ = true;
    if (callLocalUser_)
        hiddenEvents.hidden_event_types->push_back(EventType::CallSelectAnswer);

    auto loadHiddenEventsForRoom = [this, &txn, &hiddenEvents](const std::string &roomId) {
        if (auto raw = getAccountDataByType(txn, std::string(KOMAI_HIDDEN_EVENTS_TYPE), roomId)) {
            if (auto content = parseHiddenEventsFromRawAccountData(*raw)) {
                hiddenEvents = std::move(*content);
            }
        }
    };

    loadHiddenEventsForRoom("");
    loadHiddenEventsForRoom(room_id);

    return std::find(hiddenEvents.hidden_event_types->begin(),
                     hiddenEvents.hidden_event_types->end(),
                     std::visit([](const auto &ev) { return ev.type; }, e)) !=
           hiddenEvents.hidden_event_types->end();
}

MatrixStore::MatrixStore(const QString &userId, QObject *parent)
  : QObject{parent}
  , localUserId_{userId}
  , db(std::make_unique<CacheDb>())
{
    connect(
      this, &MatrixStore::userKeysUpdate, this, &MatrixStore::updateUserKeys, Qt::QueuedConnection);
    connect(
      this,
      &MatrixStore::verificationStatusChanged,
      this,
      [this](const std::string &u) {
          if (u == localUserId_.toStdString()) {
              auto status = verificationStatus(u);
              emit selfVerificationStatusChanged();
          }
      },
      Qt::QueuedConnection);
    setup();
}
