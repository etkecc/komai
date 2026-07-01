// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Client-side image resampling.
//!
//! The Qt Quick scene graph downscales textures with a low-quality GPU filter,
//! so full-view media looked soft next to colour-managed viewers. We instead
//! hand Qt a pre-downscaled image produced here with a Lanczos3 filter (the
//! same class Chromium uses), which recovers noticeably more edge detail.

use image::RgbaImage;
use image::imageops::{FilterType, resize};

/// Lanczos3-downscale a tightly packed RGBA8888 buffer to `dst_w` x `dst_h`.
///
/// `pixels` must hold exactly `src_w * src_h * 4` bytes (no row padding). The
/// caller (the C++ image provider) decodes and colour-manages the source into
/// sRGB first, so this is a pure resample: no colour conversion, no re-encode.
/// Returns the packed RGBA result, or an empty vector on any inconsistency so
/// the caller can fall back to the original image.
pub fn lanczos_resize_rgba(
    pixels: &[u8],
    src_w: u32,
    src_h: u32,
    dst_w: u32,
    dst_h: u32,
) -> Vec<u8> {
    if src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0 {
        return Vec::new();
    }
    let expected = src_w as usize * src_h as usize * 4;
    if pixels.len() != expected {
        return Vec::new();
    }
    let Some(src) = RgbaImage::from_raw(src_w, src_h, pixels.to_vec()) else {
        return Vec::new();
    };
    resize(&src, dst_w, dst_h, FilterType::Lanczos3).into_raw()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn downscales_to_requested_size() {
        let src = vec![0u8; 4 * 4 * 4];
        assert_eq!(lanczos_resize_rgba(&src, 4, 4, 2, 2).len(), 2 * 2 * 4);
    }

    #[test]
    fn rejects_mismatched_buffer() {
        assert!(lanczos_resize_rgba(&[0u8; 10], 4, 4, 2, 2).is_empty());
    }

    #[test]
    fn rejects_zero_dimensions() {
        let src = vec![0u8; 4 * 4 * 4];
        assert!(lanczos_resize_rgba(&src, 4, 4, 0, 2).is_empty());
    }
}
