// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Extract MatrixEventMediaSummary rows from image/video/audio/file
//! message contents, the MediaCaption helper trait, and the various
//! small media-source / thumbnail / numeric conversion helpers.

use super::*;

pub(super) fn media_for_image(content: &ImageMessageEventContent) -> MatrixEventMediaSummary {
    let info = content.info.as_deref();
    let thumbnail_source = info.and_then(|info| info.thumbnail_source.clone());

    MatrixEventMediaSummary {
        media_url: media_source_url(&content.source),
        thumbnail_url: thumbnail_url_or_primary(thumbnail_source.as_ref(), &content.source),
        file_name: content.filename().to_owned(),
        mime_type: info.and_then(|info| info.mimetype.clone()).unwrap_or_default(),
        media_width: info.and_then(|info| info.width).map_or(0, uint_to_u64),
        media_height: info.and_then(|info| info.height).map_or(0, uint_to_u64),
        media_duration_ms: 0,
        media_size_bytes: info.and_then(|info| info.size).map_or(0, uint_to_u64),
        blurhash: info.and_then(|info| info.blurhash.clone()).unwrap_or_default(),
        media_is_encrypted: media_source_is_encrypted(&content.source),
        thumbnail_is_encrypted: thumbnail_is_encrypted_or_primary(
            thumbnail_source.as_ref(),
            &content.source,
        ),
        source: Some(content.source.clone()),
        thumbnail_source,
    }
}

pub(super) fn media_for_video(content: &VideoMessageEventContent) -> MatrixEventMediaSummary {
    let info = content.info.as_deref();
    let thumbnail_source = info.and_then(|info| info.thumbnail_source.clone());

    MatrixEventMediaSummary {
        media_url: media_source_url(&content.source),
        thumbnail_url: thumbnail_source
            .as_ref()
            .map(media_source_url)
            .unwrap_or_default(),
        file_name: content.filename().to_owned(),
        mime_type: info.and_then(|info| info.mimetype.clone()).unwrap_or_default(),
        media_width: info.and_then(|info| info.width).map_or(0, uint_to_u64),
        media_height: info.and_then(|info| info.height).map_or(0, uint_to_u64),
        media_duration_ms: info
            .and_then(|info| info.duration)
            .map_or(0, duration_to_millis_u64),
        media_size_bytes: info.and_then(|info| info.size).map_or(0, uint_to_u64),
        blurhash: info.and_then(|info| info.blurhash.clone()).unwrap_or_default(),
        media_is_encrypted: media_source_is_encrypted(&content.source),
        thumbnail_is_encrypted: thumbnail_source
            .as_ref()
            .map(media_source_is_encrypted)
            .unwrap_or(false),
        source: Some(content.source.clone()),
        thumbnail_source,
    }
}

pub(super) fn media_for_audio(content: &AudioMessageEventContent) -> MatrixEventMediaSummary {
    let info = content.info.as_deref();

    MatrixEventMediaSummary {
        media_url: media_source_url(&content.source),
        thumbnail_url: String::new(),
        file_name: content.filename().to_owned(),
        mime_type: info.and_then(|info| info.mimetype.clone()).unwrap_or_default(),
        media_width: 0,
        media_height: 0,
        media_duration_ms: info
            .and_then(|info| info.duration)
            .map_or(0, duration_to_millis_u64),
        media_size_bytes: info.and_then(|info| info.size).map_or(0, uint_to_u64),
        blurhash: String::new(),
        media_is_encrypted: media_source_is_encrypted(&content.source),
        thumbnail_is_encrypted: false,
        source: Some(content.source.clone()),
        thumbnail_source: None,
    }
}

pub(super) fn media_for_file(content: &FileMessageEventContent) -> MatrixEventMediaSummary {
    let info = content.info.as_deref();
    let thumbnail_source = info.and_then(|info| info.thumbnail_source.clone());

    MatrixEventMediaSummary {
        media_url: media_source_url(&content.source),
        thumbnail_url: thumbnail_source
            .as_ref()
            .map(media_source_url)
            .unwrap_or_default(),
        file_name: content.filename().to_owned(),
        mime_type: info.and_then(|info| info.mimetype.clone()).unwrap_or_default(),
        media_width: 0,
        media_height: 0,
        media_duration_ms: 0,
        media_size_bytes: info.and_then(|info| info.size).map_or(0, uint_to_u64),
        blurhash: String::new(),
        media_is_encrypted: media_source_is_encrypted(&content.source),
        thumbnail_is_encrypted: thumbnail_source
            .as_ref()
            .map(media_source_is_encrypted)
            .unwrap_or(false),
        source: Some(content.source.clone()),
        thumbnail_source,
    }
}

pub(super) fn caption_or_filename<T>(content: &T) -> &str
where
    T: MediaCaption,
{
    content.caption().unwrap_or_else(|| content.filename())
}

pub(super) trait MediaCaption {
    fn filename(&self) -> &str;
    fn caption(&self) -> Option<&str>;
}

impl MediaCaption for ImageMessageEventContent {
    fn filename(&self) -> &str { ImageMessageEventContent::filename(self) }
    fn caption(&self) -> Option<&str> { ImageMessageEventContent::caption(self) }
}

impl MediaCaption for VideoMessageEventContent {
    fn filename(&self) -> &str { VideoMessageEventContent::filename(self) }
    fn caption(&self) -> Option<&str> { VideoMessageEventContent::caption(self) }
}

impl MediaCaption for AudioMessageEventContent {
    fn filename(&self) -> &str { AudioMessageEventContent::filename(self) }
    fn caption(&self) -> Option<&str> { AudioMessageEventContent::caption(self) }
}

impl MediaCaption for FileMessageEventContent {
    fn filename(&self) -> &str { FileMessageEventContent::filename(self) }
    fn caption(&self) -> Option<&str> { FileMessageEventContent::caption(self) }
}

pub(super) fn sticker_media_source_to_media_source(
    source: &matrix_sdk::ruma::events::sticker::StickerMediaSource,
) -> Option<MediaSource> {
    match source {
        matrix_sdk::ruma::events::sticker::StickerMediaSource::Plain(url) => {
            Some(MediaSource::Plain(url.clone()))
        }
        _ => None,
    }
}

pub(super) fn media_source_url(source: &MediaSource) -> String {
    match source {
        MediaSource::Plain(uri) => uri.to_string(),
        MediaSource::Encrypted(file) => file.url.to_string(),
    }
}

pub(super) fn media_source_is_encrypted(source: &MediaSource) -> bool {
    matches!(source, MediaSource::Encrypted(_))
}

pub(super) fn thumbnail_url_or_primary(
    thumbnail_source: Option<&MediaSource>,
    primary_source: &MediaSource,
) -> String {
    thumbnail_source
        .map(media_source_url)
        .unwrap_or_else(|| media_source_url(primary_source))
}

pub(super) fn thumbnail_is_encrypted_or_primary(
    thumbnail_source: Option<&MediaSource>,
    primary_source: &MediaSource,
) -> bool {
    thumbnail_source
        .map(media_source_is_encrypted)
        .unwrap_or_else(|| media_source_is_encrypted(primary_source))
}

pub(super) fn opt_uint_to_u64(value: Option<UInt>) -> u64 {
    value.map_or(0, uint_to_u64)
}

pub(super) fn uint_to_u64(value: UInt) -> u64 {
    u64::from(value)
}

pub(super) fn duration_to_millis_u64(duration: std::time::Duration) -> u64 {
    u64::try_from(duration.as_millis()).unwrap_or(u64::MAX)
}
