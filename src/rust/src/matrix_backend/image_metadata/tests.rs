// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use bytes::{BufMut, BytesMut};

fn jpeg_mime() -> Mime {
    "image/jpeg".parse().unwrap()
}
fn png_mime() -> Mime {
    "image/png".parse().unwrap()
}
fn webp_mime() -> Mime {
    "image/webp".parse().unwrap()
}

fn synthetic_jpeg_with_app1() -> Vec<u8> {
    // Minimal JPEG with intact framing: SOI + APP0 (JFIF) + APP1 (Exif…) +
    // SOS + EOI. We don't bother encoding real pixel data — img-parts only
    // walks the segment markers, and `image`'s decoder isn't asked here.
    let mut buf = BytesMut::new();
    buf.put_slice(&[0xFF, 0xD8]);
    buf.put_slice(&[
        0xFF, 0xE0, 0x00, 0x10, b'J', b'F', b'I', b'F', 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x00,
    ]);
    buf.put_slice(&[
        0xFF, 0xE1, 0x00, 0x0C, b'E', b'x', b'i', b'f', 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF,
    ]);
    buf.put_slice(&[
        0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00,
    ]);
    buf.put_slice(&[0xFF, 0xD9]);
    buf.to_vec()
}

#[test]
fn strip_jpeg_removes_app1() {
    let input = synthetic_jpeg_with_app1();
    let parsed = Jpeg::from_bytes(Bytes::copy_from_slice(&input)).expect("parse fixture");
    assert!(parsed.segment_by_marker(markers::APP1).is_some());

    let stripped = strip_image_metadata(input, &jpeg_mime());

    let after = Jpeg::from_bytes(Bytes::copy_from_slice(&stripped)).expect("parse stripped");
    assert!(after.segment_by_marker(markers::APP1).is_none());
    assert!(after.segment_by_marker(markers::APP0).is_some());
}

fn synthetic_png_with_text() -> Vec<u8> {
    // PNG: signature + IHDR + tEXt + IEND. We compute a real CRC32 — img-parts
    // validates it on read.
    let signature = [0x89, b'P', b'N', b'G', 0x0D, 0x0A, 0x1A, 0x0A];

    fn png_crc(kind: &[u8; 4], data: &[u8]) -> u32 {
        const POLY: u32 = 0xedb88320;
        let mut crc: u32 = 0xffffffff;
        for &byte in kind.iter().chain(data.iter()) {
            crc ^= byte as u32;
            for _ in 0..8 {
                crc = if crc & 1 != 0 { (crc >> 1) ^ POLY } else { crc >> 1 };
            }
        }
        !crc
    }

    fn chunk(kind: &[u8; 4], data: &[u8]) -> Vec<u8> {
        let mut out = Vec::with_capacity(12 + data.len());
        out.extend_from_slice(&(data.len() as u32).to_be_bytes());
        out.extend_from_slice(kind);
        out.extend_from_slice(data);
        out.extend_from_slice(&png_crc(kind, data).to_be_bytes());
        out
    }

    let ihdr_data = [
        0, 0, 0, 1, // width = 1
        0, 0, 0, 1, // height = 1
        8, // bit depth
        0, // color type: greyscale
        0, 0, 0, // compression, filter, interlace
    ];

    let mut out = Vec::new();
    out.extend_from_slice(&signature);
    out.extend(chunk(b"IHDR", &ihdr_data));
    out.extend(chunk(b"tEXt", b"Software\0Komai test fixture"));
    out.extend(chunk(b"IEND", &[]));
    out
}

#[test]
fn strip_png_removes_text_chunks() {
    let input = synthetic_png_with_text();
    let parsed = Png::from_bytes(Bytes::copy_from_slice(&input)).expect("parse fixture");
    assert!(parsed.chunk_by_type(*b"tEXt").is_some());

    let stripped = strip_image_metadata(input, &png_mime());

    let after = Png::from_bytes(Bytes::copy_from_slice(&stripped)).expect("parse stripped");
    assert!(after.chunk_by_type(*b"tEXt").is_none());
    assert!(after.chunk_by_type(*b"IHDR").is_some());
}

#[test]
fn unsupported_format_passes_through_unchanged() {
    let gif: Mime = "image/gif".parse().unwrap();
    let bytes = vec![1, 2, 3, 4, 5];
    let out = strip_image_metadata(bytes.clone(), &gif);
    assert_eq!(out, bytes);
}

#[test]
fn non_image_mime_passes_through_unchanged() {
    let pdf: Mime = "application/pdf".parse().unwrap();
    let bytes = b"this is not an image".to_vec();
    let out = strip_image_metadata(bytes.clone(), &pdf);
    assert_eq!(out, bytes);
}

#[test]
fn malformed_jpeg_falls_through_to_original() {
    let bytes = b"not a real jpeg".to_vec();
    let out = strip_image_metadata(bytes.clone(), &jpeg_mime());
    assert_eq!(out, bytes);
}

#[test]
fn malformed_webp_falls_through_to_original() {
    let bytes = vec![0u8; 32];
    let out = strip_image_metadata(bytes.clone(), &webp_mime());
    assert_eq!(out, bytes);
}

/// Build a real, decodable JPEG that carries an EXIF `Orientation = 6`
/// (rotate 90° CW). We build it by encoding a tiny solid-color image and
/// then splicing in a hand-crafted APP1 EXIF segment — which is enough for
/// the `image` crate's decoder to surface a non-default orientation.
fn jpeg_with_orientation_6() -> Vec<u8> {
    use image::{ImageFormat, RgbImage};
    // Tiny portrait-ish image: 4 wide, 8 tall, solid grey.
    let img = RgbImage::from_pixel(4, 8, image::Rgb([128, 128, 128]));
    let mut encoded = Vec::new();
    let encoder = image::codecs::jpeg::JpegEncoder::new_with_quality(&mut encoded, 90);
    image::DynamicImage::ImageRgb8(img)
        .write_with_encoder(encoder)
        .expect("encode synthetic jpeg");
    debug_assert_eq!(image::guess_format(&encoded).ok(), Some(ImageFormat::Jpeg));

    // Build a minimal APP1 EXIF segment with one IFD0 entry: Orientation = 6.
    // Layout: APP1 marker (FF E1) + segment length (big-endian u16, including
    // the length field itself) + "Exif\0\0" + TIFF header + IFD0.
    let mut exif_payload = Vec::new();
    exif_payload.extend_from_slice(b"Exif\0\0");
    let tiff_start = exif_payload.len();
    // TIFF header — big-endian byte order, magic 0x002A, IFD0 offset = 8 bytes.
    exif_payload.extend_from_slice(b"MM");
    exif_payload.extend_from_slice(&0x002Au16.to_be_bytes());
    exif_payload.extend_from_slice(&0x00000008u32.to_be_bytes());
    // IFD0: 1 entry, then entry, then next-IFD offset = 0.
    exif_payload.extend_from_slice(&1u16.to_be_bytes()); // entry count
    // Entry: tag = 0x0112 (Orientation), type = 3 (SHORT), count = 1, value = 6 (left-aligned in 4-byte slot).
    exif_payload.extend_from_slice(&0x0112u16.to_be_bytes());
    exif_payload.extend_from_slice(&3u16.to_be_bytes());
    exif_payload.extend_from_slice(&1u32.to_be_bytes());
    exif_payload.extend_from_slice(&[0x00, 0x06, 0x00, 0x00]); // SHORT 6, padded
    exif_payload.extend_from_slice(&0u32.to_be_bytes()); // next-IFD offset
    let _ = tiff_start; // reserved for future debug

    let segment_len = (exif_payload.len() + 2) as u16; // +2 includes the length field itself
    let mut app1 = Vec::new();
    app1.extend_from_slice(&[0xFF, 0xE1]);
    app1.extend_from_slice(&segment_len.to_be_bytes());
    app1.extend_from_slice(&exif_payload);

    // Splice APP1 right after SOI (first 2 bytes) and before whatever
    // comes next (typically APP0 JFIF from the encoder).
    let (soi, rest) = encoded.split_at(2);
    let mut spliced = Vec::with_capacity(encoded.len() + app1.len());
    spliced.extend_from_slice(soi);
    spliced.extend_from_slice(&app1);
    spliced.extend_from_slice(rest);
    spliced
}

#[test]
fn rotated_jpeg_is_re_encoded_without_exif_and_pixel_baked() {
    let input = jpeg_with_orientation_6();

    // Sanity: the decoder reports Orientation::Rotate90 (the in-pixels
    // transform you'd apply to display correctly) for an EXIF tag of 6.
    let oriented = read_orientation(&input, ImageFormat::Jpeg).expect("orientation read");
    assert_ne!(oriented, Orientation::NoTransforms);

    let stripped = strip_image_metadata(input.clone(), &jpeg_mime());
    assert_ne!(stripped, input, "rotated input must be re-encoded, not pass-through");

    // After our pipeline, the orientation tag should be gone (decoder reports
    // NoTransforms) and the image must still decode cleanly.
    let after_orientation = read_orientation(&stripped, ImageFormat::Jpeg)
        .expect("re-encoded jpeg readable");
    assert_eq!(
        after_orientation,
        Orientation::NoTransforms,
        "re-encoded output should have no remaining EXIF orientation"
    );
    let _decoded = image::load_from_memory_with_format(&stripped, ImageFormat::Jpeg)
        .expect("re-encoded jpeg must decode");

    // And no APP1 segment should remain in the container either.
    let parsed =
        Jpeg::from_bytes(Bytes::copy_from_slice(&stripped)).expect("re-encoded parses as JPEG");
    assert!(
        parsed.segment_by_marker(markers::APP1).is_none(),
        "re-encoded JPEG must not carry an APP1 segment"
    );
}
