// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <QThread>
#include <QTimer>

#include "EventAccessors.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "Utils.h"
#include "cache/Cache.h"

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

    MegolmSessionIndex index(room_id_, e.content);

    auto asCacheEntry = [&idx](olm::DecryptionResult &&event) {
        auto event_ptr = new olm::DecryptionResult(std::move(event));
        decryptedEvents_.insert(idx, event_ptr);
        return event_ptr;
    };

    auto decryptionResult = olm::decryptEvent(index, e);

    if (decryptionResult.error) {
        switch (decryptionResult.error) {
        case olm::DecryptionErrorCode::MissingSession:
        case olm::DecryptionErrorCode::MissingSessionIndex: {
            nhlog::crypto()->info("Could not find inbound megolm session ({}, {}, {})",
                                  index.room_id,
                                  index.session_id,
                                  e.sender);

            requestSession(e, false);
            break;
        }
        case olm::DecryptionErrorCode::DbError:
            nhlog::db()->critical("failed to retrieve megolm session with index ({}, {})",
                                  index.room_id,
                                  index.session_id,
                                  decryptionResult.error_message.value_or(""));
            break;
        case olm::DecryptionErrorCode::DecryptionFailed:
            nhlog::crypto()->critical("failed to decrypt message with index ({},  {}): {}",
                                      index.room_id,
                                      index.session_id,
                                      decryptionResult.error_message.value_or(""));
            break;
        case olm::DecryptionErrorCode::ParsingFailed:
            break;
        case olm::DecryptionErrorCode::ReplayAttack:
            nhlog::crypto()->critical(
              "Replay attack while decryptiong event {} in room {} from {}!",
              e.event_id,
              room_id_,
              e.sender);
            break;
        case olm::DecryptionErrorCode::NoError:
            // unreachable
            break;
        }
        return asCacheEntry(std::move(decryptionResult));
    }

    auto encInfo = mtx::accessors::file(decryptionResult.event.value());
    if (encInfo)
        emit newEncryptedImage(encInfo.value());
    encInfo = mtx::accessors::thumbnail_file(decryptionResult.event.value());
    if (encInfo)
        emit newEncryptedImage(encInfo.value());

    auto cachedResult = asCacheEntry(std::move(decryptionResult));
    if (cachedResult->event) {
        if (auto idx = cache::getTimelineIndex(room_id_, e.event_id);
            idx && *idx >= first && *idx <= last) {
            const auto externalIdx = toExternalIdx(*idx);
            QTimer::singleShot(
              0, this, [this, externalIdx]() { emit dataChanged(externalIdx, externalIdx); });
        }
    }

    return cachedResult;
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
            http::client()->get_event(room_id_,
                                      index.id,
                                      [this, relatedTo = std::string(related_to), id = index.id](
                                        const mtx::events::collections::TimelineEvents &timeline,
                                        mtx::http::RequestErr err) {
                                          if (err) {
                                              nhlog::net()->error(
                                                "Failed to retrieve event with id {}, which was "
                                                "requested to show the replyTo for event {}",
                                                id,
                                                relatedTo);
                                              return;
                                          }
                                          emit eventFetched(id, relatedTo, timeline);
                                      });
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
