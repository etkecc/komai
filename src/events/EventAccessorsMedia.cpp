// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventAccessors.h"

#include <mtx/events/collections.hpp>

namespace {

struct EventBody
{
    template<class T>
    const std::string *operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires(decltype(e) t) { t.content.body.value(); })
            return e.content.body ? &e.content.body.value() : nullptr;
        else if constexpr (requires(decltype(e) t) { std::string{t.content.body}; })
            return &e.content.body;
        return nullptr;
    }
};

struct EventFormattedBody
{
    template<class T>
    const std::string *operator()(const mtx::events::RoomEvent<T> &e)
    {
        if constexpr (requires { T::formatted_body; }) {
            if (e.content.format == "org.matrix.custom.html")
                return &e.content.formatted_body;
        }
        return nullptr;
    }
};

struct EventFile
{
    template<class T>
    const std::optional<mtx::crypto::EncryptedFile> *operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::file; })
            return &e.content.file;
        return nullptr;
    }
};

struct EventThumbnailFile
{
    template<class T>
    std::optional<mtx::crypto::EncryptedFile> operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.thumbnail_file; })
            return e.content.info.thumbnail_file;
        return std::nullopt;
    }
};

struct EventUrl
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::url; }) {
            if (auto file = EventFile{}(e); file && *file)
                return (*file)->url;
            return e.content.url;
        }
        return "";
    }
};

struct EventThumbnailUrl
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.thumbnail_url; }) {
            if (auto file = EventThumbnailFile{}(e))
                return file->url;
            return e.content.info.thumbnail_url;
        }
        return "";
    }
};

struct EventDuration
{
    template<class T>
    uint64_t operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.duration; }) {
            return e.content.info.duration;
        }
        return 0;
    }
};

struct EventBlurhash
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.blurhash; }) {
            return e.content.info.blurhash;
        }
        return "";
    }
};

struct EventFilename
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &)
    {
        return "";
    }
    std::string operator()(const mtx::events::RoomEvent<mtx::events::msg::Audio> &e)
    {
        // body may be the original filename
        return e.content.body;
    }
    std::string operator()(const mtx::events::RoomEvent<mtx::events::msg::Video> &e)
    {
        // body may be the original filename
        return e.content.body;
    }
    std::string operator()(const mtx::events::RoomEvent<mtx::events::msg::Image> &e)
    {
        // body may be the original filename
        return e.content.body;
    }
    std::string operator()(const mtx::events::RoomEvent<mtx::events::msg::File> &e)
    {
        // body may be the original filename
        if (!e.content.filename.empty())
            return e.content.filename;
        return e.content.body;
    }
};

struct EventMimeType
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.mimetype; }) {
            return e.content.info.mimetype;
        }
        return "";
    }
};

struct EventFilesize
{
    template<class T>
    int64_t operator()(const mtx::events::RoomEvent<T> &e)
    {
        if constexpr (requires { e.content.info.size; }) {
            return e.content.info.size;
        }
        return 0;
    }
};

struct EventMediaHeight
{
    template<class T>
    uint64_t operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.h; }) {
            return e.content.info.h;
        }
        return -1;
    }
};

struct EventMediaWidth
{
    template<class T>
    uint64_t operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.h; }) {
            return e.content.info.w;
        }
        return -1;
    }
};
} // namespace

std::string
mtx::accessors::filename(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventFilename{}, event);
}

std::string
mtx::accessors::body(const mtx::events::collections::TimelineEvents &event)
{
    auto body = std::visit(EventBody{}, event);
    return body ? *body : std::string{};
}

std::string
mtx::accessors::formatted_body(const mtx::events::collections::TimelineEvents &event)
{
    auto body = std::visit(EventFormattedBody{}, event);
    return body ? *body : std::string{};
}

QString
mtx::accessors::formattedBodyWithFallback(const mtx::events::collections::TimelineEvents &event)
{
    auto formatted = formatted_body(event);
    if (!formatted.empty())
        return QString::fromStdString(formatted);
    else
        return QString::fromStdString(body(event))
          .toHtmlEscaped()
          .replace(QLatin1String("\n"), QLatin1String("<br>"));
}

std::optional<mtx::crypto::EncryptedFile>
mtx::accessors::file(const mtx::events::collections::TimelineEvents &event)
{
    auto temp = std::visit(EventFile{}, event);
    if (temp)
        return *temp;
    else
        return {};
}

std::optional<mtx::crypto::EncryptedFile>
mtx::accessors::thumbnail_file(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventThumbnailFile{}, event);
}

std::string
mtx::accessors::url(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventUrl{}, event);
}

std::string
mtx::accessors::thumbnail_url(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventThumbnailUrl{}, event);
}

uint64_t
mtx::accessors::duration(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventDuration{}, event);
}

std::string
mtx::accessors::blurhash(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventBlurhash{}, event);
}

std::string
mtx::accessors::mimetype(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMimeType{}, event);
}

int64_t
mtx::accessors::filesize(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventFilesize{}, event);
}

uint64_t
mtx::accessors::media_height(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMediaHeight{}, event);
}

uint64_t
mtx::accessors::media_width(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMediaWidth{}, event);
}
