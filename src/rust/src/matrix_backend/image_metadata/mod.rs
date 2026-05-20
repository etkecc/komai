// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Best-effort stripper for image-container metadata (EXIF, IPTC, XMP, text
//! chunks) on the client, before bytes are uploaded to a homeserver. Operates
//! at the container level — JPEG segments, PNG chunks, WebP RIFF chunks — so
//! pixel data is preserved exactly. ICC color profiles are kept (color
//! accuracy, not personal data); animation chunks are kept.
//!
//! ## Orientation handling
//!
//! Phone photos commonly carry an EXIF `Orientation` tag (e.g. 6 = "rotate
//! 90° CW"). Stripping the EXIF block as-is would leave the pixels in their
//! native sensor orientation, which is what gave us the upside-down regression.
//! For images whose orientation tag is non-default, we fall back to a decode →
//! `apply_orientation` → re-encode pipeline. Re-encoding is lossless for PNG
//! and WebP; for JPEG it is lossy, so we use quality 92 to keep visible loss
//! minimal. The resulting bytes have no EXIF block at all.
//!
//! ## Failure mode
//!
//! Unsupported formats (HEIC/HEIF, AVIF, GIF, SVG, …) pass through unchanged.
//! Any parse/encode failure passes the original bytes through and logs a
//! warning — never fail an upload because of a strip bug.

use bytes::Bytes;
use image::{DynamicImage, ImageDecoder, ImageFormat, ImageReader, metadata::Orientation};
use img_parts::{
    jpeg::{Jpeg, markers},
    png::Png,
    webp::WebP,
};
use mime::Mime;
use std::io::Cursor;

pub fn strip_image_metadata(data: Vec<u8>, mime: &Mime) -> Vec<u8> {
    if mime.type_() != mime::IMAGE {
        return data;
    }

    let format = match mime.subtype().as_str() {
        "jpeg" | "jpg" => ImageFormat::Jpeg,
        "png" => ImageFormat::Png,
        "webp" => ImageFormat::WebP,
        _ => return data,
    };

    let original_len = data.len();
    let orientation = read_orientation(&data, format);
    let needs_rotation = orientation
        .map(|o| o != Orientation::NoTransforms)
        .unwrap_or(false);

    let result = if needs_rotation {
        re_encode_with_orientation(&data, format, orientation.unwrap())
    } else {
        strip_container(&data, format)
    };

    match result {
        Ok(stripped) => {
            tracing::debug!(
                mime = %mime,
                original_len,
                stripped_len = stripped.len(),
                rotated = needs_rotation,
                "Stripped image metadata before upload"
            );
            stripped
        }
        Err(err) => {
            tracing::warn!(
                mime = %mime,
                original_len,
                error = %err,
                rotated = needs_rotation,
                "Failed to strip image metadata; uploading original bytes"
            );
            data
        }
    }
}

fn read_orientation(data: &[u8], format: ImageFormat) -> Option<Orientation> {
    let mut reader = ImageReader::new(Cursor::new(data));
    reader.set_format(format);
    let mut decoder = reader.into_decoder().ok()?;
    decoder.orientation().ok()
}

fn re_encode_with_orientation(
    data: &[u8],
    format: ImageFormat,
    orientation: Orientation,
) -> Result<Vec<u8>, String> {
    let mut reader = ImageReader::new(Cursor::new(data));
    reader.set_format(format);
    let decoder = reader
        .into_decoder()
        .map_err(|e| format!("decoder for orientation fix: {e}"))?;
    let mut img = DynamicImage::from_decoder(decoder)
        .map_err(|e| format!("decode pixels for orientation fix: {e}"))?;
    img.apply_orientation(orientation);

    let mut out = Vec::with_capacity(data.len());
    match format {
        ImageFormat::Jpeg => {
            // Quality 92: high enough that side-by-side compression artifacts
            // are very hard to spot on a phone photo, but well below the steep
            // file-size growth above q=95.
            let encoder = image::codecs::jpeg::JpegEncoder::new_with_quality(&mut out, 92);
            img.write_with_encoder(encoder)
                .map_err(|e| format!("re-encode JPEG: {e}"))?;
        }
        ImageFormat::Png => {
            let encoder = image::codecs::png::PngEncoder::new(&mut out);
            img.write_with_encoder(encoder)
                .map_err(|e| format!("re-encode PNG: {e}"))?;
        }
        ImageFormat::WebP => {
            // The pure-Rust WebP encoder in the `image` crate is lossless
            // only. Output may be larger than a lossy original, but this path
            // only triggers for WebP files with non-default orientation, which
            // are extremely rare in practice.
            let encoder = image::codecs::webp::WebPEncoder::new_lossless(&mut out);
            img.write_with_encoder(encoder)
                .map_err(|e| format!("re-encode WebP: {e}"))?;
        }
        other => return Err(format!("unsupported re-encode format: {other:?}")),
    }
    Ok(out)
}

fn strip_container(data: &[u8], format: ImageFormat) -> Result<Vec<u8>, String> {
    match format {
        ImageFormat::Jpeg => strip_jpeg(data),
        ImageFormat::Png => strip_png(data),
        ImageFormat::WebP => strip_webp(data),
        other => Err(format!("strip not supported for {other:?}")),
    }
}

fn strip_jpeg(data: &[u8]) -> Result<Vec<u8>, String> {
    let mut jpeg = Jpeg::from_bytes(Bytes::copy_from_slice(data))
        .map_err(|e| format!("parse JPEG: {e}"))?;

    // Strip every APPn marker except APP0 (JFIF) and APP2 (ICC). Also strip
    // COM (comments). EXIF/XMP live in APP1, IPTC in APP13, Adobe in APP14,
    // and various vendor MakerNote / dual-image / depth-map data in APP3-15.
    for marker in (markers::APP0..=markers::APP15).chain(std::iter::once(markers::COM)) {
        if marker == markers::APP0 || marker == markers::APP2 {
            continue;
        }
        jpeg.remove_segments_by_marker(marker);
    }

    let bytes = jpeg.encoder().bytes();
    Ok(bytes.to_vec())
}

fn strip_png(data: &[u8]) -> Result<Vec<u8>, String> {
    let mut png = Png::from_bytes(Bytes::copy_from_slice(data))
        .map_err(|e| format!("parse PNG: {e}"))?;

    // Drop EXIF, all text chunks (which can carry XMP), and the last-modified
    // timestamp. iCCP (ICC profile) and pHYs (physical pixel size) are kept.
    for kind in [b"eXIf", b"iTXt", b"tEXt", b"zTXt", b"tIME"] {
        png.remove_chunks_by_type(*kind);
    }

    let bytes = png.encoder().bytes();
    Ok(bytes.to_vec())
}

fn strip_webp(data: &[u8]) -> Result<Vec<u8>, String> {
    let mut webp = WebP::from_bytes(Bytes::copy_from_slice(data))
        .map_err(|e| format!("parse WebP: {e}"))?;

    // Note the trailing space in `XMP `: WebP/RIFF chunk IDs are exactly 4 bytes.
    webp.remove_chunks_by_id(*b"EXIF");
    webp.remove_chunks_by_id(*b"XMP ");

    let bytes = webp.encoder().bytes();
    Ok(bytes.to_vec())
}

#[cfg(test)]
mod tests;
