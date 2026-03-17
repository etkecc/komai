// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <chrono>

#include <QGuiApplication>

#include "events/EventAccessors.h"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"

void
TimelineModel::setCurrentIndex(int index)
{
    setCurrentIndex(index, false);
}

void
TimelineModel::setCurrentIndex(int index, bool ignoreInactiveState)
{
    auto oldIndex = idToIndex(currentId);
    currentId     = indexToId(index);
    if (index != oldIndex)
        emit currentIndexChanged(index);

    if (!ignoreInactiveState &&
        (!QGuiApplication::focusWindow() || !QGuiApplication::focusWindow()->isActive() ||
         MainWindow::instance()->windowForRoom(roomId()) != QGuiApplication::focusWindow()))
        return;

    if (!currentId.startsWith('m')) {
        auto oldReadIndex =
          cache::getEventIndex(roomId().toStdString(), currentReadId.toStdString());
        auto nextEventIndexAndId =
          cache::lastInvisibleEventAfter(roomId().toStdString(), currentId.toStdString());

        if (nextEventIndexAndId && (!oldReadIndex || *oldReadIndex < nextEventIndexAndId->first)) {
            readEvent(nextEventIndexAndId->second);
            currentReadId = QString::fromStdString(nextEventIndexAndId->second);
        }
    }
}

void
TimelineModel::readEvent(const std::string &id)
{
    http::client()->read_event(
      room_id_.toStdString(),
      id,
      [this, newId = id, oldId = currentReadId](mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to read_event ({}, {})", room_id_.toStdString(), newId);

              ChatPage::instance()->callFunctionOnGuiThread([this, newId, oldId] {
                  if (currentReadId.toStdString() == newId)
                      this->currentReadId = oldId;
              });
          } else {
              ChatPage::instance()->callFunctionOnGuiThread([this, newId] {
                  cache::markRoomReadLocally(room_id_.toStdString(), newId);

                  const auto previousNotificationCount = notification_count;
                  const auto previousHighlightCount    = highlight_count;
                  notification_count                   = 0;
                  highlight_count                      = 0;
                  fullyReadEventId_                    = newId;
                  emit fullyReadEventIdChanged();

                  if (previousNotificationCount != 0 || previousHighlightCount != 0)
                      emit notificationsChanged();
              });
          }
      },
      !UserSettings::instance()->timelineReadReceiptsEnabled());
}

int
TimelineModel::idToIndex(const QString &id) const
{
    if (id.isEmpty())
        return -1;

    auto idx = events.idToIndex(id.toStdString());
    if (idx)
        return events.size() - *idx - 1;
    else
        return -1;
}

QString
TimelineModel::indexToId(int index) const
{
    auto id = events.indexToId(events.size() - index - 1);
    return id ? QString::fromStdString(*id) : QLatin1String("");
}

// Read receipts can move between events, so refresh the visible room timeline from cache.
void
TimelineModel::markEventsAsRead(const std::vector<QString> &)
{
    if (rowCount() == 0)
        return;

    emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

void
TimelineModel::markRoomAsRead()
{
    setCurrentIndex(0, true);
}

void
TimelineModel::updateLastReadId(const QString &currentRoomId)
{
    if (currentRoomId == room_id_) {
        last_event_id = cache::getFullyReadEventId(room_id_.toStdString());
        auto lastVisibleEventIndexAndId =
          cache::lastVisibleEvent(room_id_.toStdString(), last_event_id);
        if (lastVisibleEventIndexAndId) {
            fullyReadEventId_ = lastVisibleEventIndexAndId->second;
            emit fullyReadEventIdChanged();
        }
    }
}

void
TimelineModel::lastReadIdOnWindowFocus()
{
    /* this stops it from removing the line when focusing another window
     * and from removing the line when refocusing Komai */
    if (ChatPage::instance()->isRoomActive(room_id_) &&
        cache::calculateRoomReadStatus(room_id_.toStdString())) {
        updateLastReadId(room_id_);
    }
}

/*
 * if the event2order db didn't have the messages we needed when the room was opened
 * try again after these new messages were fetched
 */
void
TimelineModel::checkAfterFetch()
{
    if (fullyReadEventId_.empty()) {
        auto lastVisibleEventIndexAndId =
          cache::lastVisibleEvent(room_id_.toStdString(), last_event_id);
        if (lastVisibleEventIndexAndId) {
            fullyReadEventId_ = lastVisibleEventIndexAndId->second;
            emit fullyReadEventIdChanged();
        }
    }
}

void
TimelineModel::showEvent(QString eventId)
{
    using namespace std::chrono_literals;
    // Direct to eventId
    if (eventId[0] == '$') {
        int idx = idToIndex(eventId);
        if (idx == -1) {
            nhlog::ui()->warn("Scrolling to event id {}, failed - no known index",
                              eventId.toStdString());
            return;
        }
        eventIdToShow = eventId;
        emit scrollTargetChanged();
        showEventTimer.start(50ms);
        return;
    }
    // to message index
    eventId       = indexToId(eventId.toInt());
    eventIdToShow = eventId;
    emit scrollTargetChanged();
    showEventTimer.start(50ms);
    return;
}

void
TimelineModel::eventShown()
{
    eventIdToShow.clear();
    emit scrollTargetChanged();
}

QString
TimelineModel::scrollTarget() const
{
    return eventIdToShow;
}

void
TimelineModel::scrollTimerEvent()
{
    if (eventIdToShow.isEmpty() || showEventTimerCounter > 3) {
        showEventTimer.stop();
        showEventTimerCounter = 0;
    } else {
        emit scrollToIndex(idToIndex(eventIdToShow));
        showEventTimerCounter++;
    }
}

QVariantMap
TimelineModel::adjacentMediaEvent(const QString &currentEventId, int direction) const
{
    auto currentIdx = events.idToIndex(currentEventId.toStdString());
    if (!currentIdx)
        return {};

    const int step  = (direction >= 0) ? 1 : -1;
    const int limit = 10000;

    for (int i = 1; i <= limit; ++i) {
        int idx     = *currentIdx + step * i;
        auto *event = events.get(idx);
        if (!event)
            break;

        auto type = qml_mtx_events::toRoomEventType(*event);
        if (type != qml_mtx_events::EventType::ImageMessage &&
            type != qml_mtx_events::EventType::Sticker &&
            type != qml_mtx_events::EventType::VideoMessage)
            continue;

        auto w      = mtx::accessors::media_width(*event);
        double prop = (w > 0) ? static_cast<double>(mtx::accessors::media_height(*event)) /
                                  static_cast<double>(w)
                              : 1.0;

        QVariantMap result;
        result[QStringLiteral("eventId")] =
          QString::fromStdString(mtx::accessors::event_id(*event));
        result[QStringLiteral("url")] = QString::fromStdString(mtx::accessors::url(*event));
        result[QStringLiteral("originalWidth")]      = QVariant::fromValue(qulonglong{w});
        result[QStringLiteral("proportionalHeight")] = QVariant::fromValue(prop);
        result[QStringLiteral("type")]               = static_cast<int>(type);
        result[QStringLiteral("duration")] =
          QVariant::fromValue(qulonglong{mtx::accessors::duration(*event)});
        result[QStringLiteral("thumbnailUrl")] =
          QString::fromStdString(mtx::accessors::thumbnail_url(*event));
        return result;
    }

    return {};
}

int
TimelineModel::countNearbyMedia(const QString &currentEventId, int direction, int limit) const
{
    auto currentIdx = events.idToIndex(currentEventId.toStdString());
    if (!currentIdx)
        return 0;

    const int step = (direction >= 0) ? 1 : -1;
    int count      = 0;

    for (int i = 1; i <= 10000 && count < limit; ++i) {
        int idx     = *currentIdx + step * i;
        auto *event = events.get(idx);
        if (!event)
            break;

        auto type = qml_mtx_events::toRoomEventType(*event);
        if (type == qml_mtx_events::EventType::ImageMessage ||
            type == qml_mtx_events::EventType::Sticker ||
            type == qml_mtx_events::EventType::VideoMessage)
            ++count;
    }

    nhlog::ui()->info("countNearbyMedia from {} dir={} limit={}: found {}",
                      currentEventId.toStdString(),
                      direction,
                      limit,
                      count);
    return count;
}

void
TimelineModel::requestKeyForEvent(const QString &id)
{
    auto encrypted_event = events.get(id.toStdString(), "", false);
    if (encrypted_event) {
        if (auto ev = std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
              encrypted_event))
            events.requestSession(*ev, true);
    }
}
