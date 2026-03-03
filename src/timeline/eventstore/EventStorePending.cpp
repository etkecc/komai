// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <QThread>
#include <QTimer>

#include <type_traits>

#include <nlohmann/json.hpp>

#include <mtx/responses/common.hpp>

#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

void
EventStore::setupPendingPipeline()
{
    connect(this, &EventStore::processPending, this, [this]() {
        if (!current_txn.empty()) {
            nhlog::ui()->debug("Already processing {}", current_txn);
            return;
        }

        auto event = cache::firstPendingMessage(room_id_);

        if (!event) {
            nhlog::ui()->debug("No event to send");
            return;
        }

        std::visit(
          [this](const auto &e) {
              const auto &txn_id = e.event_id;
              this->current_txn  = txn_id;

              if (txn_id.empty() || txn_id[0] != 'm') {
                  nhlog::ui()->debug("Invalid txn id '{}'", txn_id);
                  cache::removePendingStatus(room_id_, txn_id);
                  return;
              }

              if constexpr (mtx::events::message_content_to_type<decltype(e.content)> !=
                            mtx::events::EventType::Unsupported)
                  http::client()->send_room_message(
                    room_id_,
                    txn_id,
                    e.content,
                    [this, txn_id, e](const mtx::responses::EventId &event_id,
                                      mtx::http::RequestErr err) {
                        if (err) {
                            const int status_code = static_cast<int>(err->status_code);
                            nhlog::net()->warn("[{}] failed to send message: {} {}",
                                               txn_id,
                                               err->matrix_error.error,
                                               status_code);
                            emit messageFailed(txn_id);
                            return;
                        }

                        emit messageSent(txn_id, event_id.event_id.to_string());
                        if constexpr (std::is_same_v<decltype(e.content),
                                                     mtx::events::msg::Encrypted>) {
                            auto event = decryptEvent({room_id_, e.event_id}, e);
                            if (event->event) {
                                if (auto dec = std::get_if<mtx::events::RoomEvent<
                                      mtx::events::msg::KeyVerificationRequest>>(
                                      &event->event.value())) {
                                    emit updateFlowEventId(event_id.event_id.to_string());
                                }
                            }
                        }
                    });
          },
          event.value());
    });

    connect(
      this,
      &EventStore::messageFailed,
      this,
      [this](std::string txn_id) {
          if (current_txn == txn_id) {
              current_txn_error_count++;
              if (current_txn_error_count > 10) {
                  nhlog::ui()->debug("failing txn id '{}'", txn_id);
                  cache::removePendingStatus(room_id_, txn_id);
                  current_txn_error_count = 0;
              }
          }
          QTimer::singleShot(1000, this, [this]() {
              nhlog::ui()->debug("timeout");
              this->current_txn = "";
              emit processPending();
          });
      },
      Qt::QueuedConnection);

    connect(
      this,
      &EventStore::messageSent,
      this,
      [this](std::string txn_id, std::string event_id) {
          nhlog::ui()->debug("sent {}", txn_id);

          // Replace the event_id in pending edits/replies/redactions with the actual
          // event_id of this event. This allows one to edit and reply to events that are
          // currently pending.
          for (const auto &pending_event_id : cache::pendingEvents(room_id_)) {
              if (auto pending_event = cache::getEvent(room_id_, pending_event_id)) {
                  bool was_encrypted = false;
                  mtx::events::EncryptedEvent<mtx::events::msg::Encrypted> original_encrypted;
                  if (auto encrypted =
                        std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
                          &pending_event.value())) {
                      auto d_event = decryptEvent({room_id_, encrypted->event_id}, *encrypted);
                      if (d_event->event) {
                          was_encrypted      = true;
                          original_encrypted = std::move(*encrypted);
                          *pending_event     = std::move(*d_event->event);
                      }
                  }

                  auto relations = mtx::accessors::relations(pending_event.value());

                  // Replace the blockquote in fallback reply
                  auto related_text = std::get_if<mtx::events::RoomEvent<mtx::events::msg::Text>>(
                    &pending_event.value());
                  if (related_text && relations.reply_to() == txn_id) {
                      size_t index = related_text->content.formatted_body.find(txn_id);
                      if (index != std::string::npos) {
                          related_text->content.formatted_body.replace(
                            index, txn_id.length(), event_id);
                      }
                  }

                  bool replaced_txn = false;
                  for (mtx::common::Relation &rel : relations.relations) {
                      if (rel.event_id == txn_id) {
                          rel.event_id = event_id;
                          replaced_txn = true;
                      }
                  }

                  if (!replaced_txn)
                      continue;

                  mtx::accessors::set_relations(pending_event.value(), std::move(relations));

                  // reencrypt. This is a bit of a hack and might make people able to read the
                  // message, that were in the room at the time of sending the last pending message.
                  // That window is pretty small though, so it should be good enough. We also just
                  // fail, if there was no session. But there SHOULD always be one. Let's wait until
                  // I am proven wrong :3
                  if (was_encrypted) {
                      auto session = cache::getOutboundMegolmSession(room_id_);
                      if (!session.session)
                          continue;

                      auto doc = std::visit(
                        [this](auto &msg) {
                            return nlohmann::json{{"type", mtx::events::to_string(msg.type)},
                                                  {"content", nlohmann::json(msg.content)},
                                                  {"room_id", room_id_}};
                        },
                        pending_event.value());

                      auto data = olm::encrypt_group_message_with_session(
                        session.session, http::client()->device_id(), std::move(doc));

                      session.data.message_index =
                        olm_outbound_group_session_message_index(session.session.get());
                      cache::updateOutboundMegolmSession(room_id_, session.data, session.session);

                      original_encrypted.content = std::move(data);
                      *pending_event             = std::move(original_encrypted);
                  }

                  cache::replaceEvent(room_id_, pending_event_id, *pending_event);

                  auto idx = idToIndex(pending_event_id);

                  events_by_id_.remove({room_id_, pending_event_id});
                  if (idx)
                      events_.remove({room_id_, toInternalIdx(*idx)});
              }
          }

          http::client()->read_event(
            room_id_,
            event_id,
            [this, event_id](mtx::http::RequestErr err) {
                if (err) {
                    nhlog::net()->warn("failed to read_event ({}, {})", room_id_, event_id);
                }
            },
            !UserSettings::instance()->timelineReadReceiptsEnabled());

          auto idx = idToIndex(event_id);

          if (idx)
              emit dataChanged(*idx, *idx);

          cache::removePendingStatus(room_id_, txn_id);
          this->current_txn             = "";
          this->current_txn_error_count = 0;
          emit processPending();
      },
      Qt::QueuedConnection);
}

void
EventStore::addPending(const mtx::events::collections::TimelineEvents &event)
{
    if (this->thread() != QThread::currentThread())
        nhlog::db()->warn("{} called from a different thread!", __func__);

    cache::savePendingMessage(this->room_id_, event);
    mtx::responses::Timeline events;
    events.limited = false;
    events.events.emplace_back(event);
    handleSync(events);

    emit processPending();
}
