// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventExpiry.h"

#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

#include "Logging.h"
#include "MatrixClient.h"
#include "cache/Cache.h"
#include "timeline/TimelineModel.h"
#include "ui/MainWindow.h"

namespace {
using EventExpiryContent = mtx::events::account_data::nheko_extensions::EventExpiry;
constexpr std::string_view KOMAI_EVENT_EXPIRY_TYPE = "cc.etke.komai.event_expiry";

std::optional<EventExpiryContent>
parseEventExpiryFromRawAccountData(const std::string &eventJson)
{
    try {
        const auto parsedEvent = nlohmann::json::parse(eventJson);
        if (!parsedEvent.is_object() || !parsedEvent.contains("content"))
            return std::nullopt;

        return parsedEvent.at("content").get<EventExpiryContent>();
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

void
loadEventExpiryForRoom(const std::string &roomId, EventExpiryContent &event)
{
    if (auto raw = cache::getAccountDataByType(std::string(KOMAI_EVENT_EXPIRY_TYPE), roomId)) {
        if (auto content = parseEventExpiryFromRawAccountData(*raw))
            event = std::move(*content);
    }
}
} // namespace

void
EventExpiry::load()
{
    this->event = {};
    loadEventExpiryForRoom("", this->event);

    if (!roomid_.isEmpty())
        loadEventExpiryForRoom(roomid_.toStdString(), this->event);

    emit expireEventsAfterDaysChanged();
    emit expireEventsAfterCountChanged();
    emit protectLatestEventsChanged();
    emit expireStateEventsChanged();
}

void
EventExpiry::save()
{
    if (roomid_.isEmpty())
        http::client()->put_account_data(
          std::string(KOMAI_EVENT_EXPIRY_TYPE), event, [](mtx::http::RequestErr e) {
              if (e) {
                  nhlog::net()->error("Failed to set event expiry: {}", *e);
                  MainWindow::instance()->showNotification(
                    tr("Failed to set event expiry: %1")
                      .arg(QString::fromStdString(e->matrix_error.error)));
              }
          });
    else
        http::client()->put_room_account_data(
          roomid_.toStdString(),
          std::string(KOMAI_EVENT_EXPIRY_TYPE),
          event,
          [](mtx::http::RequestErr e) {
              if (e) {
                  nhlog::net()->error("Failed to set event expiry: {}", *e);
                  MainWindow::instance()->showNotification(
                    tr("Failed to set event expiry: %1")
                      .arg(QString::fromStdString(e->matrix_error.error)));
              }
          });
}

int
EventExpiry::expireEventsAfterDays() const
{
    return event.expire_after_ms / (1000 * 60 * 60 * 24);
}

int
EventExpiry::expireEventsAfterCount() const
{
    return event.keep_only_latest;
}

int
EventExpiry::protectLatestEvents() const
{
    return event.protect_latest;
}

bool
EventExpiry::expireStateEvents() const
{
    return !event.exclude_state_events;
}

void
EventExpiry::setExpireEventsAfterDays(int val)
{
    if (val > 0)
        this->event.expire_after_ms = std::uint64_t(val) * (1000 * 60 * 60 * 24);
    else
        this->event.expire_after_ms = 0;
    emit expireEventsAfterDaysChanged();
}

void
EventExpiry::setProtectLatestEvents(int val)
{
    if (val > 0)
        this->event.protect_latest = std::uint64_t(val);
    else
        this->event.expire_after_ms = 0;
    emit protectLatestEventsChanged();
}

void
EventExpiry::setExpireEventsAfterCount(int val)
{
    if (val > 0)
        this->event.keep_only_latest = std::uint64_t(val);
    else
        this->event.keep_only_latest = 0;
    emit expireEventsAfterCountChanged();
}

void
EventExpiry::setExpireStateEvents(bool val)
{
    this->event.exclude_state_events = !val;
    emit expireEventsAfterCountChanged();
}

#include "moc_EventExpiry.cpp"
