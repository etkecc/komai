// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use mime::Mime;
use serde_json::Value;
use std::fs;

fn optional_uint_from_value(value: Option<&Value>) -> Option<UInt> {
    value.and_then(Value::as_u64).and_then(UInt::new)
}

fn image_info_from_json(info_json: &str) -> Result<Option<Box<ImageInfo>>, String> {
    let trimmed = info_json.trim();
    if trimmed.is_empty() {
        return Ok(None);
    }

    let value: Value =
        serde_json::from_str(trimmed).map_err(|e| format!("invalid image info json: {e}"))?;
    let object = value
        .as_object()
        .ok_or_else(|| "image info json must be an object".to_owned())?;

    let mut info = ImageInfo::new();
    info.width = optional_uint_from_value(object.get("w").or_else(|| object.get("width")));
    info.height = optional_uint_from_value(object.get("h").or_else(|| object.get("height")));
    info.size = optional_uint_from_value(object.get("size"));
    info.mimetype = object
        .get("mimetype")
        .or_else(|| object.get("mimeType"))
        .and_then(Value::as_str)
        .map(ToOwned::to_owned);

    if info.width.is_none() && info.height.is_none() && info.size.is_none() && info.mimetype.is_none() {
        Ok(None)
    } else {
        Ok(Some(Box::new(info)))
    }
}

pub async fn upload_media(
    handle_id: u64,
    file_path: &str,
    mime_type: &str,
    strip_image_metadata: bool,
) -> Result<String, String> {
    let client = client_for_handle(handle_id)?;
    let file_path = file_path.trim();
    if file_path.is_empty() {
        return Err("cannot upload matrix media without a file path".to_owned());
    }

    let mime: Mime = mime_type
        .trim()
        .parse()
        .map_err(|e| format!("invalid media mime type '{mime_type}': {e}"))?;
    let data = fs::read(file_path)
        .map_err(|e| format!("failed to read media file '{file_path}': {e}"))?;
    let data = if strip_image_metadata {
        crate::matrix_backend::image_metadata::strip_image_metadata(data, &mime)
    } else {
        data
    };

    tracing::info!(
        handle_id,
        file_path,
        mime_type,
        byte_count = data.len(),
        "Uploading unencrypted matrix media via matrix-sdk backend runtime"
    );

    client
        .media()
        .upload(&mime, data, None)
        .await
        .map(|response| response.content_uri.to_string())
        .map_err(|e| format!("failed to upload matrix media via matrix-sdk: {e}"))
}

pub async fn send_room_image(
    handle_id: u64,
    room_id: &str,
    mxc_uri: &str,
    body: &str,
    filename: &str,
    info_json: &str,
    use_send_queue: bool,
) -> Result<String, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let normalized_mxc_uri = mxc_uri.trim();
    if normalized_mxc_uri.is_empty() {
        return Err("cannot send a matrix-sdk room image without an mxc uri".to_owned());
    }

    let uri_ref = <&MxcUri>::from(normalized_mxc_uri);
    uri_ref
        .validate()
        .map_err(|e| format!("invalid mxc uri '{normalized_mxc_uri}': {e}"))?;
    let uri = uri_ref.to_owned();

    let encryption_state = room
        .latest_encryption_state()
        .await
        .map_err(|e| format!("failed to inspect matrix room encryption state: {e}"))?;
    if encryption_state.is_encrypted() {
        return Err(format!(
            "cannot send an existing mxc image into encrypted room '{}'; use image-file upload instead",
            room_id.trim()
        ));
    }

    let caption = body.trim();
    let filename = filename.trim();
    let effective_filename = if filename.is_empty() {
        if caption.is_empty() {
            "image".to_owned()
        } else {
            "image".to_owned()
        }
    } else {
        filename.to_owned()
    };

    let mut content = if caption.is_empty() {
        ImageMessageEventContent::plain(effective_filename.clone(), uri.into())
    } else {
        let mut content = ImageMessageEventContent::plain(caption.to_owned(), uri.into());
        content.filename = Some(effective_filename.clone());
        content
    };
    content.info = image_info_from_json(info_json)?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        mxc_uri = normalized_mxc_uri,
        has_caption = !caption.is_empty(),
        filename = effective_filename,
        has_info = !info_json.trim().is_empty(),
        "Sending matrix-sdk room image from existing mxc uri"
    );

    super::timeline_messaging::deliver_message_content(
        &room,
        AnyMessageLikeEventContent::RoomMessage(RoomMessageEventContent::new(MessageType::Image(
            content,
        ))),
        use_send_queue,
        "room image",
    )
    .await
}
