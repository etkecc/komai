// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HiddenEvents.h"

#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

#include "Logging.h"
#include "MatrixClient.h"
#include "cache/Cache.h"
#include "timeline/TimelineModel.h"
#include "ui/MainWindow.h"

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

void
loadHiddenEventsForRoom(const std::string &roomId, HiddenEventsContent &hiddenEvents)
{
    if (auto raw = cache::getAccountDataByType(std::string(KOMAI_HIDDEN_EVENTS_TYPE), roomId)) {
        if (auto content = parseHiddenEventsFromRawAccountData(*raw)) {
            hiddenEvents = std::move(*content);
        }
    }
}
} // namespace

void
HiddenEvents::load()
{
    using namespace mtx::events;
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

    loadHiddenEventsForRoom("", hiddenEvents);

    if (!roomid_.isEmpty())
        loadHiddenEventsForRoom(roomid_.toStdString(), hiddenEvents);

    hiddenEvents_.clear();
    hiddenEvents_ = std::move(hiddenEvents.hidden_event_types.value());
    emit hiddenEventsChanged();
}

Q_INVOKABLE void
HiddenEvents::toggle(int type)
{
    auto t = qml_mtx_events::fromRoomEventType(static_cast<qml_mtx_events::EventType>(type));
    if (auto it = std::find(begin(hiddenEvents_), end(hiddenEvents_), t); it != end(hiddenEvents_))
        hiddenEvents_.erase(it);
    else
        hiddenEvents_.push_back(t);
    emit hiddenEventsChanged();
}

QVariantList
HiddenEvents::hiddenEvents() const
{
    QVariantList l;
    for (const auto &e : hiddenEvents_) {
        l.push_back(qml_mtx_events::toRoomEventType(e));
    }

    return l;
}

void
HiddenEvents::save()
{
    HiddenEventsContent hiddenEvents;
    hiddenEvents.hidden_event_types = hiddenEvents_;

    if (roomid_.isEmpty())
        http::client()->put_account_data(
          std::string(KOMAI_HIDDEN_EVENTS_TYPE), hiddenEvents, [](mtx::http::RequestErr e) {
              if (e) {
                  nhlog::net()->error("Failed to set hidden events: {}", *e);
                  MainWindow::instance()->showNotification(
                    tr("Failed to set hidden events: %1")
                      .arg(QString::fromStdString(e->matrix_error.error)));
              }
          });
    else
        http::client()->put_room_account_data(
          std::string(roomid_.toStdString()),
          std::string(KOMAI_HIDDEN_EVENTS_TYPE),
          hiddenEvents,
          [](mtx::http::RequestErr e) {
              if (e) {
                  nhlog::net()->error("Failed to set hidden events: {}", *e);
                  MainWindow::instance()->showNotification(
                    tr("Failed to set hidden events: %1")
                      .arg(QString::fromStdString(e->matrix_error.error)));
              }
          });
}

#include "moc_HiddenEvents.cpp"
