// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <algorithm>
#include <optional>

#include "timeline/TimelineEventTypes.h"
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "events/EventAccessors.h"
#include "logging/Logging.h"

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

    // Edit events should not get their own message index entry — they are resolved into the
    // original message by EventStore::get(). In encrypted rooms this check may be bypassed
    // (the event is still encrypted at index time), so the UI layer has a complementary check
    // in TimelineModel::data(QModelIndex, IsHiddenEvent) to catch those.
    if (mtx::accessors::relations(e).replaces())
        return true;

    // Do not eagerly invoke the legacy Olm/event-store decryption graph from the cache layer on
    // this migration branch. Encrypted events may slip through this index-time filter, but the UI
    // layer already has a later IsHiddenEvent guard once an event is actually materialized.

    HiddenEventsContent hiddenEvents;
    hiddenEvents.hidden_event_types = qml_mtx_events::defaultHiddenEventTypes();

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
