// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "InputBar.h"

#include "TimelineModel.h"
#include "cache/Cache.h"

void
InputBar::image(const QString &filename,
                const std::optional<mtx::crypto::EncryptedFile> &file,
                const QString &url,
                const QString &mime,
                uint64_t dsize,
                const QSize &dimensions,
                const std::optional<mtx::crypto::EncryptedFile> &thumbnailEncryptedFile,
                const QString &thumbnailUrl,
                uint64_t thumbnailSize,
                const QSize &thumbnailDimensions,
                const QString &blurhash)
{
    mtx::events::msg::Image image;
    image.info.mimetype = mime.toStdString();
    image.info.size     = dsize;
    image.info.blurhash = blurhash.toStdString();
    image.body          = filename.toStdString();
    image.info.h        = dimensions.height();
    image.info.w        = dimensions.width();

    if (file)
        image.file = file;
    else
        image.url = url.toStdString();

    if (!thumbnailUrl.isEmpty()) {
        if (thumbnailEncryptedFile)
            image.info.thumbnail_file = thumbnailEncryptedFile;
        else
            image.info.thumbnail_url = thumbnailUrl.toStdString();

        image.info.thumbnail_info.h        = thumbnailDimensions.height();
        image.info.thumbnail_info.w        = thumbnailDimensions.width();
        image.info.thumbnail_info.size     = thumbnailSize;
        image.info.thumbnail_info.mimetype = "image/png";
    }

    image.mentions  = generateMentions();
    image.relations = generateRelations();

    room->sendMessageEvent(image, mtx::events::EventType::RoomMessage);
}

void
InputBar::file(const QString &filename,
               const std::optional<mtx::crypto::EncryptedFile> &encryptedFile,
               const QString &url,
               const QString &mime,
               uint64_t dsize)
{
    mtx::events::msg::File file;
    file.info.mimetype = mime.toStdString();
    file.info.size     = dsize;
    file.body          = filename.toStdString();

    if (encryptedFile)
        file.file = encryptedFile;
    else
        file.url = url.toStdString();

    file.mentions  = generateMentions();
    file.relations = generateRelations();

    room->sendMessageEvent(file, mtx::events::EventType::RoomMessage);
}

void
InputBar::audio(const QString &filename,
                const std::optional<mtx::crypto::EncryptedFile> &file,
                const QString &url,
                const QString &mime,
                uint64_t dsize,
                uint64_t duration)
{
    mtx::events::msg::Audio audio;
    audio.info.mimetype = mime.toStdString();
    audio.info.size     = dsize;
    audio.body          = filename.toStdString();
    audio.url           = url.toStdString();

    if (duration > 0)
        audio.info.duration = duration;

    if (file)
        audio.file = file;
    else
        audio.url = url.toStdString();

    audio.mentions  = generateMentions();
    audio.relations = generateRelations();

    room->sendMessageEvent(audio, mtx::events::EventType::RoomMessage);
}

void
InputBar::video(const QString &filename,
                const std::optional<mtx::crypto::EncryptedFile> &file,
                const QString &url,
                const QString &mime,
                uint64_t dsize,
                uint64_t duration,
                const QSize &dimensions,
                const std::optional<mtx::crypto::EncryptedFile> &thumbnailEncryptedFile,
                const QString &thumbnailUrl,
                uint64_t thumbnailSize,
                const QSize &thumbnailDimensions,
                const QString &blurhash)
{
    mtx::events::msg::Video video;
    video.info.mimetype = mime.toStdString();
    video.info.size     = dsize;
    video.info.blurhash = blurhash.toStdString();
    video.body          = filename.toStdString();

    if (duration > 0)
        video.info.duration = duration;
    if (dimensions.isValid()) {
        video.info.h = dimensions.height();
        video.info.w = dimensions.width();
    }

    if (file)
        video.file = file;
    else
        video.url = url.toStdString();

    if (!thumbnailUrl.isEmpty()) {
        if (thumbnailEncryptedFile)
            video.info.thumbnail_file = thumbnailEncryptedFile;
        else
            video.info.thumbnail_url = thumbnailUrl.toStdString();

        video.info.thumbnail_info.h        = thumbnailDimensions.height();
        video.info.thumbnail_info.w        = thumbnailDimensions.width();
        video.info.thumbnail_info.size     = thumbnailSize;
        video.info.thumbnail_info.mimetype = "image/png";
    }

    video.mentions  = generateMentions();
    video.relations = generateRelations();

    room->sendMessageEvent(video, mtx::events::EventType::RoomMessage);
}

void
InputBar::sticker(QStringList descriptor)
{
    if (descriptor.size() != 3)
        return;

    auto originalPacks = cache::getImagePacks(room->roomId().toStdString(), true);

    auto source_room = descriptor[0].toStdString();
    auto state_key   = descriptor[1].toStdString();
    auto short_code  = descriptor[2].toStdString();

    for (auto &pack : originalPacks) {
        if (pack.source_room == source_room && pack.state_key == state_key &&
            pack.pack.images.contains(short_code)) {
            auto img = pack.pack.images.at(short_code);

            mtx::events::msg::StickerImage sticker{};
            sticker.info = img.info.value_or(mtx::common::ImageInfo{});
            sticker.url  = img.url;
            sticker.body = img.body.empty() ? short_code : img.body;

            // workaround for https://github.com/vector-im/element-ios/issues/2353
            sticker.info.thumbnail_url           = sticker.url;
            sticker.info.thumbnail_info.mimetype = sticker.info.mimetype;
            sticker.info.thumbnail_info.size     = sticker.info.size;
            sticker.info.thumbnail_info.h        = sticker.info.h;
            sticker.info.thumbnail_info.w        = sticker.info.w;

            sticker.mentions  = generateMentions();
            sticker.relations = generateRelations();

            room->sendMessageEvent(sticker, mtx::events::EventType::Sticker);
            break;
        }
    }
}
