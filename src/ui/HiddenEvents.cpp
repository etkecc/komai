// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HiddenEvents.h"

#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

#include "cache/Cache.h"
#include "logging/Logging.h"
#include "timeline/TimelineEventTypes.h"
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

void
notifyHiddenEventsSaveUnavailable()
{
    nhlog::ui()->warn("Hidden event persistence is not migrated to matrix-sdk yet");
    MainWindow::instance()->showNotification(HiddenEvents::tr(
      "Saving hidden events is not available yet during the matrix-sdk migration."));
}
} // namespace

void
HiddenEvents::load()
{
    using namespace mtx::events;
    HiddenEventsContent hiddenEvents;
    hiddenEvents.hidden_event_types = qml_mtx_events::defaultHiddenEventTypes();

    if (cache::isAvailable() && cache::isDatabaseReady()) {
        loadHiddenEventsForRoom("", hiddenEvents);

        if (!roomid_.isEmpty())
            loadHiddenEventsForRoom(roomid_.toStdString(), hiddenEvents);
    }

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
    notifyHiddenEventsSaveUnavailable();
}

#include "moc_HiddenEvents.cpp"
