// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <optional>

#include <mtx/responses/media.hpp>

#include "Logging.h"
#include "MatrixClient.h"
#include "RoomlistModel.h"
#include "TimelineModel.h"
#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "utils/Utils.h"

namespace {
struct nonesuch
{
    ~nonesuch()                      = delete;
    nonesuch(nonesuch const &)       = delete;
    void operator=(nonesuch const &) = delete;
};

namespace detail {
template<class Default, class AlwaysVoid, template<class...> class Op, class... Args>
struct detector
{
    using value_t = std::false_type;
    using type    = Default;
};

template<class Default, template<class...> class Op, class... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...>
{
    using value_t = std::true_type;
    using type    = Op<Args...>;
};

} // namespace detail

template<template<class...> class Op, class... Args>
using is_detected = typename detail::detector<nonesuch, void, Op, Args...>::value_t;

template<class Content>
using file_t = decltype(Content::file);

template<class Content>
using url_t = decltype(Content::url);

template<class Content>
using body_t = decltype(Content::body);

template<class Content>
using formatted_body_t = decltype(Content::formatted_body);

template<typename T>
static constexpr bool
messageWithFileAndUrl(const mtx::events::Event<T> &)
{
    return is_detected<file_t, T>::value && is_detected<url_t, T>::value;
}

template<typename T>
static constexpr void
removeReplyFallback(mtx::events::Event<T> &e)
{
    if constexpr (is_detected<body_t, T>::value) {
        if constexpr (std::is_same_v<std::optional<std::string>,
                                     std::remove_cv_t<decltype(e.content.body)>>) {
            if (e.content.body) {
                e.content.body = utils::stripReplyFromBody(e.content.body);
            }
        } else if constexpr (std::is_same_v<std::string,
                                            std::remove_cv_t<decltype(e.content.body)>>) {
            e.content.body = utils::stripReplyFromBody(e.content.body);
        }
    }

    if constexpr (is_detected<formatted_body_t, T>::value) {
        if (e.content.format == "org.matrix.custom.html") {
            e.content.formatted_body = utils::stripReplyFromFormattedBody(e.content.formatted_body);
        }
    }
}
} // namespace

void
TimelineViewManager::forwardMessageToRoom(mtx::events::collections::TimelineEvents const *e,
                                          QString roomId)
{
    auto room                                                = rooms_->getRoomById(roomId);
    auto content                                             = mtx::accessors::url(*e);
    std::optional<mtx::crypto::EncryptedFile> encryptionInfo = mtx::accessors::file(*e);

    if (encryptionInfo && !cache::isRoomEncrypted(roomId.toStdString())) {
        http::client()->download(
          content,
          [this, roomId, e, encryptionInfo](const std::string &res,
                                            const std::string &content_type,
                                            const std::string &originalFilename,
                                            mtx::http::RequestErr err) {
              if (err)
                  return;

              auto data =
                mtx::crypto::to_string(mtx::crypto::decrypt_file(res, encryptionInfo.value()));

              http::client()->upload(
                data,
                content_type,
                originalFilename,
                [this, roomId, e](const mtx::responses::ContentURI &res,
                                  mtx::http::RequestErr err) mutable {
                    if (err) {
                        nhlog::net()->warn("failed to upload media: {} {} ({})",
                                           err->matrix_error.error,
                                           to_string(err->matrix_error.errcode),
                                           static_cast<int>(err->status_code));
                        return;
                    }

                    std::visit(
                      [this, roomId, url = res.content_uri](auto ev) {
                          using namespace mtx::events;
                          if constexpr (EventType::RoomMessage ==
                                          message_content_to_type<decltype(ev.content)> ||
                                        EventType::Sticker ==
                                          message_content_to_type<decltype(ev.content)>) {
                              if constexpr (messageWithFileAndUrl(ev)) {
                                  ev.content.relations.relations.clear();
                                  ev.content.file.reset();
                                  ev.content.url = url;
                              }

                              if (auto room = rooms_->getRoomById(roomId)) {
                                  removeReplyFallback(ev);
                                  ev.content.relations.relations.clear();
                                  room->sendMessageEvent(ev.content,
                                                         mtx::events::EventType::RoomMessage);
                              }
                          }
                      },
                      *e);
                });

              return;
          });

        return;
    }

    std::visit(
      [room](auto e) {
          constexpr auto type = mtx::events::message_content_to_type<decltype(e.content)>;
          if constexpr (type == mtx::events::EventType::RoomMessage ||
                        type == mtx::events::EventType::Sticker) {
              e.content.relations.relations.clear();
              removeReplyFallback(e);
              room->sendMessageEvent(e.content, type);
          }
      },
      *e);
}
