// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <QThread>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "utils/Utils.h"

QCache<EventStore::IdIndex, EventStore::LegacyDecryptionResult> EventStore::decryptedEvents_{1000};
QCache<EventStore::IdIndex, mtx::events::collections::TimelineEvents> EventStore::events_by_id_{
  1000};
QCache<EventStore::Index, mtx::events::collections::TimelineEvents> EventStore::events_{1000};

namespace {
int
parsePositiveIntEnv(const char *name, int fallbackValue)
{
    const auto value = qEnvironmentVariable(name).trimmed();
    if (value.isEmpty())
        return fallbackValue;

    bool ok          = false;
    const auto asInt = value.toInt(&ok);
    if (!ok || asInt <= 0)
        return fallbackValue;

    return asInt;
}

}

EventStore::EventStore(std::string room_id, QObject *)
  : room_id_(std::move(room_id))
  , initialWindowSize_(parsePositiveIntEnv("KOMAI_TIMELINE_INITIAL_WINDOW", 12))
  , expandChunkSize_(parsePositiveIntEnv("KOMAI_TIMELINE_EXPAND_CHUNK", 50))
{
    auto range = cache::getTimelineRange(room_id_);

    if (range) {
        applyInitialWindowFromRange(range->first, range->last);
    }

    connect(
      this,
      &EventStore::eventFetched,
      this,
      [this](const std::string &id,
             const std::string &relatedTo,
             mtx::events::collections::TimelineEvents timeline) {
          cache::storeEvent(room_id_, id, {timeline});

          if (!relatedTo.empty()) {
              if (relatedTo == "pins") {
                  emit pinsChanged();
              } else {
                  auto idx = idToIndex(relatedTo);
                  if (idx)
                      emit dataChanged(*idx, *idx);
              }
          }
      },
      Qt::QueuedConnection);

    connect(
      this,
      &EventStore::oldMessagesRetrieved,
      this,
      [this](const mtx::responses::Messages &res) {
          if (res.end.empty() || cache::previousBatchToken(room_id_) == res.end) {
              noMoreMessages = true;
              emit fetchedMore();
              return;
          }

          uint64_t newFirst = cache::saveOldMessages(room_id_, res);
          if (newFirst == first) {
              fetchMore();
          } else {
              if (this->last != std::numeric_limits<uint64_t>::max()) {
                  auto oldFirst = this->first;
                  emit beginInsertRows(toExternalIdx(newFirst), toExternalIdx(this->first - 1));
                  this->first   = newFirst;
                  this->dbFirst = newFirst;
                  emit endInsertRows();
                  emit dataChanged(toExternalIdx(oldFirst), toExternalIdx(oldFirst));
                  emit fetchedMore();
              } else {
                  auto range = cache::getTimelineRange(room_id_);

                  if (range && range->last - range->first != 0) {
                      emit beginInsertRows(0, int(range->last - range->first));
                      this->first   = range->first;
                      this->dbFirst = range->first;
                      this->last    = range->last;
                      emit endInsertRows();
                      emit fetchedMore();
                  } else {
                      fetchMore();
                  }
              }
          }
      },
      Qt::QueuedConnection);
    setupPendingPipeline();
}

void
EventStore::clearTimeline()
{
    emit beginResetModel();

    cache::clearTimeline(room_id_);
    auto range = cache::getTimelineRange(room_id_);
    if (range) {
        nhlog::db()->info("Range {} {}", range->last, range->first);
        applyInitialWindowFromRange(range->first, range->last);
    } else {
        this->first   = std::numeric_limits<uint64_t>::max();
        this->dbFirst = std::numeric_limits<uint64_t>::max();
        this->last    = std::numeric_limits<uint64_t>::max();
    }
    nhlog::ui()->info("Range {} {}", this->last, this->first);

    decryptedEvents_.clear();
    events_.clear();
    noMoreMessages = false;

    emit endResetModel();
}

namespace {
template<class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}

static void
handle_room_verification(EventStore *self, const mtx::events::collections::TimelineEvents &event)
{
    std::visit(
      overloaded{
        [self](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationRequest> &msg) {
            nhlog::db()->debug("handle_room_verification: Request");
            emit self->startDMVerification(msg);
        },
        [](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationCancel> &msg) {
            nhlog::db()->debug("handle_room_verification: Cancel");
            ChatPage::instance()->receivedDeviceVerificationCancel(msg.content);
        },
        [](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationAccept> &msg) {
            nhlog::db()->debug("handle_room_verification: Accept");
            ChatPage::instance()->receivedDeviceVerificationAccept(msg.content);
        },
        [](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationKey> &msg) {
            nhlog::db()->debug("handle_room_verification: Key");
            ChatPage::instance()->receivedDeviceVerificationKey(msg.content);
        },
        [](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationMac> &msg) {
            nhlog::db()->debug("handle_room_verification: Mac");
            ChatPage::instance()->receivedDeviceVerificationMac(msg.content);
        },
        [](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationReady> &msg) {
            nhlog::db()->debug("handle_room_verification: Ready");
            ChatPage::instance()->receivedDeviceVerificationReady(msg.content);
        },
        [](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationDone> &msg) {
            nhlog::db()->debug("handle_room_verification: Done");
            ChatPage::instance()->receivedDeviceVerificationDone(msg.content);
        },
        [](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationStart> &msg) {
            nhlog::db()->debug("handle_room_verification: Start");
            ChatPage::instance()->receivedDeviceVerificationStart(msg.content, msg.sender);
        },
        [](const auto &) {},
      },
      event);
}

void
EventStore::handleSync(const mtx::responses::Timeline &events)
{
    if (this->thread() != QThread::currentThread())
        nhlog::db()->warn("{} called from a different thread!", __func__);

    auto range = cache::getTimelineRange(room_id_);
    if (!range) {
        emit beginResetModel();
        this->first   = std::numeric_limits<uint64_t>::max();
        this->dbFirst = std::numeric_limits<uint64_t>::max();
        this->last    = std::numeric_limits<uint64_t>::max();

        decryptedEvents_.clear();
        events_.clear();
        noMoreMessages = false;
        emit endResetModel();
        return;
    }

    if (events.limited) {
        emit beginResetModel();
        applyInitialWindowFromRange(range->first, range->last);

        decryptedEvents_.clear();
        events_.clear();
        noMoreMessages = false;
        emit endResetModel();
    } else if (range->last > this->last) {
        emit beginInsertRows(toExternalIdx(this->last + 1), toExternalIdx(range->last));
        this->last = range->last;
        emit endInsertRows();
    }

    for (const auto &event : events.events) {
        std::set<std::string> relates_to;
        std::string edited_event;
        if (auto redaction =
              std::get_if<mtx::events::RedactionEvent<mtx::events::msg::Redaction>>(&event)) {
            // fixup reactions
            auto redacted = events_by_id_.object({room_id_, redaction->redacts});
            if (redacted) {
                auto id = mtx::accessors::relations(*redacted);
                if (id.annotates()) {
                    auto idx = idToIndex(id.annotates()->event_id);
                    if (idx) {
                        events_by_id_.remove({room_id_, redaction->redacts});
                        events_.remove({room_id_, toInternalIdx(*idx)});
                        emit dataChanged(*idx, *idx);
                    }
                }
            }

            relates_to.insert(redaction->redacts);
        } else {
            for (const auto &r : mtx::accessors::relations(event).relations) {
                relates_to.insert(r.event_id);

                if (r.rel_type == mtx::common::RelationType::Replace)
                    edited_event = r.event_id;
            }
        }

        for (const auto &relates_to_id : relates_to) {
            auto idx = cache::getTimelineIndex(room_id_, relates_to_id);
            if (idx) {
                events_by_id_.remove({room_id_, relates_to_id});
                decryptedEvents_.remove({room_id_, relates_to_id});
                events_.remove({room_id_, *idx});
                emit dataChanged(toExternalIdx(*idx), toExternalIdx(*idx));
            }
        }

        if (auto txn_id = mtx::accessors::transaction_id(event); !txn_id.empty()) {
            auto idx = cache::getTimelineIndex(room_id_, mtx::accessors::event_id(event));
            if (idx) {
                Index index{room_id_, *idx};
                events_.remove(index);
                emit dataChanged(toExternalIdx(*idx), toExternalIdx(*idx));
            }
        }

        if (!edited_event.empty()) {
            for (const auto &downstream_event : cache::relatedEvents(room_id_, edited_event)) {
                auto idx = cache::getTimelineIndex(room_id_, downstream_event);
                if (idx) {
                    emit dataChanged(toExternalIdx(*idx), toExternalIdx(*idx));
                }
            }
        }

        // decrypting and checking some encrypted messages
        if (auto encrypted =
              std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(&event)) {
            auto d_event = decryptEvent({room_id_, encrypted->event_id}, *encrypted);
            if (d_event->event &&
                mtx::accessors::sender(*d_event->event) != utils::localUser().toStdString()) {
                handle_room_verification(this, *d_event->event);
            }
        } else {
            // workaround Element not encrypting verification events anymore
            if (mtx::accessors::sender(event) != utils::localUser().toStdString()) {
                handle_room_verification(this, event);
            }
        }
    }
}

#include "moc_EventStore.cpp"
