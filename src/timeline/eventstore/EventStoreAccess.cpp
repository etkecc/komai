// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <QThread>

#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"

mtx::events::collections::TimelineEvents const *
EventStore::get(int idx, bool decrypt)
{
    if (this->thread() != QThread::currentThread())
        nhlog::db()->warn("{} called from a different thread!", __func__);

    Index index{room_id_, toInternalIdx(idx)};
    if (index.idx > last || index.idx < first)
        return nullptr;

    auto event_ptr = events_.object(index);
    if (!event_ptr) {
        auto event_id = cache::getTimelineEventId(room_id_, index.idx);
        if (!event_id)
            return nullptr;

        std::optional<mtx::events::collections::TimelineEvents> event;
        auto edits_ = edits(*event_id);
        if (edits_.empty())
            event = cache::getEvent(room_id_, *event_id);
        else
            // Resolve the original message to its latest edit. Note: the returned event will
            // have a different event_id than *event_id (the indexed one). The UI's IsHiddenEvent
            // check relies on this mismatch to distinguish resolved edits from edit events that
            // wrongly got their own index (encrypted rooms).
            event = mtx::events::collections::TimelineEvents{edits_.back()};

        if (!event)
            return nullptr;
        else
            event_ptr = new mtx::events::collections::TimelineEvents(std::move(*event));
        events_.insert(index, event_ptr);
    }

    if (decrypt) {
        if (auto encrypted =
              std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(event_ptr)) {
            auto decrypted = decryptEvent({room_id_, encrypted->event_id}, *encrypted);
            if (decrypted->event)
                return &*decrypted->event;
        }
    }

    return event_ptr;
}

std::optional<int>
EventStore::idToIndex(std::string_view id) const
{
    if (this->thread() != QThread::currentThread())
        nhlog::db()->warn("{} called from a different thread!", __func__);

    auto idx = cache::getTimelineIndex(room_id_, id);
    if (idx)
        return toExternalIdx(*idx);
    else
        return std::nullopt;
}

std::optional<std::string>
EventStore::indexToId(int idx) const
{
    if (this->thread() != QThread::currentThread())
        nhlog::db()->warn("{} called from a different thread!", __func__);

    return cache::getTimelineEventId(room_id_, toInternalIdx(idx));
}

olm::DecryptionResult const *
EventStore::decryptEvent(const IdIndex &idx,
                         const mtx::events::EncryptedEvent<mtx::events::msg::Encrypted> &e)
{
    if (auto cachedEvent = decryptedEvents_.object(idx))
        return cachedEvent;

    auto asCacheEntry = [&idx](olm::DecryptionResult &&event) {
        auto event_ptr = new olm::DecryptionResult(std::move(event));
        decryptedEvents_.insert(idx, event_ptr);
        return event_ptr;
    };

    nhlog::crypto()->warn(
      "Refusing to decrypt legacy EventStore event {} in room {}; this flow is not migrated "
      "to the matrix-sdk backend yet",
      e.event_id,
      room_id_);

    return asCacheEntry(
      {olm::DecryptionErrorCode::MissingSession,
       std::optional<std::string>("Legacy EventStore decryption is not migrated yet"),
       std::nullopt});
}

mtx::events::collections::TimelineEvents const *
EventStore::get(const std::string &id,
                std::string_view related_to,
                bool decrypt,
                bool resolve_edits)
{
    if (this->thread() != QThread::currentThread())
        nhlog::db()->warn("{} called from a different thread!", __func__);

    if (id.empty())
        return nullptr;

    IdIndex index{room_id_, id};
    if (resolve_edits) {
        auto edits_ = edits(index.id);
        if (!edits_.empty()) {
            index.id       = mtx::accessors::event_id(edits_.back());
            auto event_ptr = new mtx::events::collections::TimelineEvents(std::move(edits_.back()));
            events_by_id_.insert(index, event_ptr);
        }
    }

    auto event_ptr = events_by_id_.object(index);
    if (!event_ptr) {
        auto event = cache::getEvent(room_id_, index.id);
        if (!event) {
            nhlog::ui()->warn(
              "Refusing to fetch missing related event {} for {}; this legacy EventStore path "
              "is not migrated to the matrix-sdk backend yet",
              index.id,
              related_to);
            return nullptr;
        }
        event_ptr = new mtx::events::collections::TimelineEvents(std::move(*event));
        events_by_id_.insert(index, event_ptr);
    }

    if (decrypt) {
        if (auto encrypted =
              std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(event_ptr)) {
            auto decrypted = decryptEvent(index, *encrypted);
            if (decrypted->event)
                return &*decrypted->event;
        }
    }

    return event_ptr;
}

olm::DecryptionErrorCode
EventStore::decryptionError(std::string id)
{
    if (this->thread() != QThread::currentThread())
        nhlog::db()->warn("{} called from a different thread!", __func__);

    if (id.empty())
        return olm::DecryptionErrorCode::NoError;

    IdIndex index{room_id_, std::move(id)};
    auto edits_ = edits(index.id);
    if (!edits_.empty()) {
        index.id       = mtx::accessors::event_id(edits_.back());
        auto event_ptr = new mtx::events::collections::TimelineEvents(std::move(edits_.back()));
        events_by_id_.insert(index, event_ptr);
    }

    auto event_ptr = events_by_id_.object(index);
    if (!event_ptr) {
        auto event = cache::getEvent(room_id_, index.id);
        if (!event) {
            return olm::DecryptionErrorCode::NoError;
        }
        event_ptr = new mtx::events::collections::TimelineEvents(std::move(*event));
        events_by_id_.insert(index, event_ptr);
    }

    if (auto encrypted =
          std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(event_ptr)) {
        auto decrypted = decryptEvent(index, *encrypted);
        return decrypted->error;
    }

    return olm::DecryptionErrorCode::NoError;
}
