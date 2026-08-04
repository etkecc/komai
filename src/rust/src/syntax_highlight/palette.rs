// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Contrast-aware syntax highlighting palettes.
//!
//! syntect's bundled themes are authored against one fixed background. Komai
//! renders code blocks on whatever surface the active theme provides, so a
//! bundled theme's colors land at arbitrary contrast — most visibly on light
//! themes, where `base16-ocean.light` reuses the dark variant's pastel accents.
//!
//! Instead, a [`Theme`] is built per background: start from a curated palette
//! for the background's polarity, then push each color's lightness until it
//! clears [`MIN_CONTRAST_RATIO`] against that background. Hue and saturation
//! survive the adjustment, so tokens stay recognizable and distinct from each
//! other; only their lightness moves.

use std::collections::HashMap;
use std::str::FromStr;
use std::sync::{LazyLock, Mutex};

use syntect::highlighting::{Color, ScopeSelectors, StyleModifier, Theme, ThemeItem, ThemeSettings};

/// Minimum WCAG contrast ratio between a syntax token and the code background.
///
/// 4.5:1 is WCAG AA for body text. Editor themes conventionally sit lower, so
/// this deliberately mutes saturated hues in exchange for legibility.
pub(super) const MIN_CONTRAST_RATIO: f64 = 4.5;

/// Colors for each role the scope table below assigns.
///
/// Roles mirror the ones `base16-ocean` distinguishes, so highlighting keeps
/// the same visual language it had when syntect's bundled themes were used.
struct SyntaxPalette {
    plain: u32,
    comment: u32,
    string: u32,
    constant: u32,
    keyword: u32,
    function: u32,
    class: u32,
    variable: u32,
    support: u32,
    embedded: u32,
}

/// Saturated dark-on-light hues. Deliberately not pastels: contrast correction
/// darkens colors, and low-saturation inputs converge toward a common grey,
/// which costs the ability to tell one token kind from another.
const LIGHT_PALETTE: SyntaxPalette = SyntaxPalette {
    plain: 0x1f2328,
    comment: 0x57606a,
    string: 0x0a7d32,
    constant: 0x953800,
    keyword: 0x8250df,
    function: 0x0550ae,
    class: 0xb35900,
    variable: 0xcf222e,
    support: 0x0e7490,
    embedded: 0x7d4e00,
};

/// `base16-ocean.dark`'s accents, which already read well on dark surfaces.
/// Correction mostly leaves these alone and lifts the dim comment grey.
const DARK_PALETTE: SyntaxPalette = SyntaxPalette {
    plain: 0xc0c5ce,
    comment: 0x65737e,
    string: 0xa3be8c,
    constant: 0xd08770,
    keyword: 0xb48ead,
    function: 0x8fa1b3,
    class: 0xebcb8b,
    variable: 0xbf616a,
    support: 0x96b5b4,
    embedded: 0xab7967,
};

// ---------------------------------------------------------------------------
// Color math
// ---------------------------------------------------------------------------

pub(super) fn rgb(hex: u32) -> Color {
    Color {
        r: ((hex >> 16) & 0xff) as u8,
        g: ((hex >> 8) & 0xff) as u8,
        b: (hex & 0xff) as u8,
        a: 0xff,
    }
}

/// WCAG relative luminance.
fn relative_luminance(color: Color) -> f64 {
    let channel = |v: u8| {
        let v = f64::from(v) / 255.0;
        if v <= 0.03928 {
            v / 12.92
        } else {
            ((v + 0.055) / 1.055).powf(2.4)
        }
    };
    0.2126 * channel(color.r) + 0.7152 * channel(color.g) + 0.0722 * channel(color.b)
}

/// WCAG contrast ratio, always >= 1.0.
pub(super) fn contrast_ratio(a: Color, b: Color) -> f64 {
    let (la, lb) = (relative_luminance(a), relative_luminance(b));
    let (hi, lo) = if la > lb { (la, lb) } else { (lb, la) };
    (hi + 0.05) / (lo + 0.05)
}

/// True when text on this background should be lightened rather than darkened.
fn is_dark_background(bg: Color) -> bool {
    contrast_ratio(bg, rgb(0xffffff)) > contrast_ratio(bg, rgb(0x000000))
}

fn to_hsl(color: Color) -> (f64, f64, f64) {
    let (r, g, b) = (
        f64::from(color.r) / 255.0,
        f64::from(color.g) / 255.0,
        f64::from(color.b) / 255.0,
    );
    let max = r.max(g).max(b);
    let min = r.min(g).min(b);
    let lightness = (max + min) / 2.0;

    if (max - min).abs() < f64::EPSILON {
        return (0.0, 0.0, lightness);
    }

    let delta = max - min;
    let saturation = if lightness > 0.5 {
        delta / (2.0 - max - min)
    } else {
        delta / (max + min)
    };
    let hue = if (max - r).abs() < f64::EPSILON {
        ((g - b) / delta).rem_euclid(6.0)
    } else if (max - g).abs() < f64::EPSILON {
        (b - r) / delta + 2.0
    } else {
        (r - g) / delta + 4.0
    };

    (hue / 6.0, saturation, lightness)
}

fn from_hsl(hue: f64, saturation: f64, lightness: f64) -> Color {
    if saturation <= f64::EPSILON {
        let v = (lightness * 255.0).round() as u8;
        return Color { r: v, g: v, b: v, a: 0xff };
    }

    let q = if lightness < 0.5 {
        lightness * (1.0 + saturation)
    } else {
        lightness + saturation - lightness * saturation
    };
    let p = 2.0 * lightness - q;

    let channel = |t: f64| {
        let t = t.rem_euclid(1.0);
        let v = if t < 1.0 / 6.0 {
            p + (q - p) * 6.0 * t
        } else if t < 0.5 {
            q
        } else if t < 2.0 / 3.0 {
            p + (q - p) * (2.0 / 3.0 - t) * 6.0
        } else {
            p
        };
        (v * 255.0).round() as u8
    };

    Color {
        r: channel(hue + 1.0 / 3.0),
        g: channel(hue),
        b: channel(hue - 1.0 / 3.0),
        a: 0xff,
    }
}

/// Move `color`'s lightness toward the background's opposite extreme until it
/// reaches `target` contrast, keeping hue and saturation.
///
/// A mid-tone background can be too close to both extremes for any color to
/// reach the target; the search then returns the best available extreme rather
/// than failing.
fn enforce_contrast(color: Color, bg: Color, target: f64) -> Color {
    if contrast_ratio(color, bg) >= target {
        return color;
    }

    let lighten = is_dark_background(bg);
    let (hue, saturation, lightness) = to_hsl(color);
    let (mut lo, mut hi) = if lighten { (lightness, 1.0) } else { (0.0, lightness) };
    let mut best = if lighten { rgb(0xffffff) } else { rgb(0x000000) };

    // 24 halvings resolve lightness far below one 8-bit step.
    for _ in 0..24 {
        let mid = f64::midpoint(lo, hi);
        let candidate = from_hsl(hue, saturation, mid);
        if contrast_ratio(candidate, bg) >= target {
            best = candidate;
            // Keep the color as close to its original lightness as the target
            // allows, so we darken/lighten no more than necessary.
            if lighten {
                hi = mid;
            } else {
                lo = mid;
            }
        } else if lighten {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    best
}

// ---------------------------------------------------------------------------
// Theme construction
// ---------------------------------------------------------------------------

/// Scope selectors per role, mirroring `base16-ocean`'s assignments.
fn scope_table(palette: &SyntaxPalette) -> [(&'static str, u32); 13] {
    [
        ("variable.parameter.function", palette.plain),
        ("comment, punctuation.definition.comment", palette.comment),
        (
            "punctuation.definition.string, punctuation.definition.variable, \
             punctuation.definition.parameters, punctuation.definition.array",
            palette.plain,
        ),
        ("keyword.operator, meta.separator", palette.plain),
        ("keyword, storage, meta.selector, markup.italic", palette.keyword),
        (
            "variable, entity.name.tag, string.other.link, markup.list, markup.deleted",
            palette.variable,
        ),
        (
            "entity.name.function, meta.require, support.function.any-method, \
             variable.function, keyword.other.special-method, \
             entity.other.attribute-name.id, punctuation.definition.entity, \
             entity.name.section, markup.heading punctuation.definition.heading",
            palette.function,
        ),
        (
            "support.class, entity.name.class, entity.name.type.class, meta.class",
            palette.class,
        ),
        (
            "support.function, constant.other.color, string.regexp, constant.character.escape",
            palette.support,
        ),
        (
            "string, constant.other.symbol, entity.other.inherited-class, \
             markup.raw.inline, markup.inserted",
            palette.string,
        ),
        (
            "constant.numeric, constant, entity.other.attribute-name, keyword.other.unit, \
             markup.bold, meta.link, meta.image, markup.quote",
            palette.constant,
        ),
        ("punctuation.section.embedded, variable.interpolation", palette.embedded),
        ("markup.changed", palette.keyword),
    ]
}

fn build_theme(bg: Color) -> Theme {
    let palette = if is_dark_background(bg) { &DARK_PALETTE } else { &LIGHT_PALETTE };
    let correct = |hex: u32| enforce_contrast(rgb(hex), bg, MIN_CONTRAST_RATIO);

    let scopes = scope_table(palette)
        .into_iter()
        .filter_map(|(selector, color)| {
            // Selectors are compile-time constants; a parse failure means this
            // table is malformed, so drop the entry rather than poison the theme.
            ScopeSelectors::from_str(selector).ok().map(|scope| ThemeItem {
                scope,
                style: StyleModifier {
                    foreground: Some(correct(color)),
                    background: None,
                    font_style: None,
                },
            })
        })
        .collect();

    Theme {
        name: Some("Komai".to_owned()),
        settings: ThemeSettings {
            background: Some(bg),
            foreground: Some(correct(palette.plain)),
            ..ThemeSettings::default()
        },
        scopes,
        ..Theme::default()
    }
}

/// Themes are rebuilt only when a background is seen for the first time, so a
/// theme switch costs one construction rather than one per highlighted message.
static THEME_CACHE: LazyLock<Mutex<HashMap<u32, Theme>>> =
    LazyLock::new(|| Mutex::new(HashMap::new()));

/// Parse `#rrggbb` (or `rrggbb`). Falls back to white, matching the default
/// light surface, when the caller passes something unusable.
pub(super) fn parse_background(text: &str) -> Color {
    let digits = text.trim().trim_start_matches('#');
    if digits.len() == 6 {
        if let Ok(value) = u32::from_str_radix(digits, 16) {
            return rgb(value);
        }
    }
    rgb(0xffffff)
}

/// A contrast-corrected theme for `bg`, built once per distinct background.
pub(super) fn theme_for_background(bg: Color) -> Theme {
    let key = (u32::from(bg.r) << 16) | (u32::from(bg.g) << 8) | u32::from(bg.b);

    // A poisoned lock would only mean a previous build panicked; rebuilding is
    // cheap and correct, so fall back to an uncached theme instead of failing.
    let Ok(mut cache) = THEME_CACHE.lock() else {
        return build_theme(bg);
    };
    cache.entry(key).or_insert_with(|| build_theme(bg)).clone()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ratio(hex: u32, bg: u32) -> f64 {
        contrast_ratio(rgb(hex), rgb(bg))
    }

    #[test]
    fn contrast_ratio_matches_wcag_reference() {
        // Black on white is the definitional 21:1; equal colors are 1:1.
        assert!((ratio(0x000000, 0xffffff) - 21.0).abs() < 0.01);
        assert!((ratio(0x777777, 0x777777) - 1.0).abs() < 0.001);
    }

    #[test]
    fn hsl_roundtrip_preserves_color() {
        for hex in [0x0a7d32, 0xcf222e, 0x8250df, 0x1f2328, 0xffffff, 0x000000] {
            let color = rgb(hex);
            let (h, s, l) = to_hsl(color);
            let back = from_hsl(h, s, l);
            assert_eq!((color.r, color.g, color.b), (back.r, back.g, back.b), "{hex:#08x}");
        }
    }

    #[test]
    fn enforce_contrast_reaches_target_on_light_backgrounds() {
        // Catppuccin Latte's alternate base, the surface issue #258 reported on.
        let bg = rgb(0xccd0da);
        for hex in [0x0a7d32, 0xcf222e, 0x8250df, 0x0550ae, 0x57606a, 0x0e7490] {
            let fixed = enforce_contrast(rgb(hex), bg, MIN_CONTRAST_RATIO);
            assert!(
                contrast_ratio(fixed, bg) >= MIN_CONTRAST_RATIO - 0.01,
                "{hex:#08x} reached only {:.2}",
                contrast_ratio(fixed, bg)
            );
        }
    }

    #[test]
    fn enforce_contrast_reaches_target_on_dark_backgrounds() {
        let bg = rgb(0x313244);
        for hex in [0xa3be8c, 0x65737e, 0xbf616a, 0x8fa1b3, 0xd08770] {
            let fixed = enforce_contrast(rgb(hex), bg, MIN_CONTRAST_RATIO);
            assert!(
                contrast_ratio(fixed, bg) >= MIN_CONTRAST_RATIO - 0.01,
                "{hex:#08x} reached only {:.2}",
                contrast_ratio(fixed, bg)
            );
        }
    }

    #[test]
    fn enforce_contrast_leaves_compliant_colors_untouched() {
        let bg = rgb(0xffffff);
        let color = rgb(0x1f2328);
        assert_eq!(enforce_contrast(color, bg, MIN_CONTRAST_RATIO), color);
    }

    #[test]
    fn enforce_contrast_preserves_hue() {
        let bg = rgb(0xccd0da);
        // A pastel green far below the target still reads as green afterwards.
        let fixed = enforce_contrast(rgb(0xa3be8c), bg, MIN_CONTRAST_RATIO);
        assert!(fixed.g > fixed.r && fixed.g > fixed.b, "{fixed:?} is no longer green");
    }

    #[test]
    fn worst_case_background_still_reaches_the_target() {
        // Mid greys are the hardest surfaces to sit on, being far from both
        // extremes. Around #777777 white alone no longer clears 4.5:1, so the
        // search has to pick the darkening direction to stay compliant.
        let bg = rgb(0x777777);
        for hex in [0xa3be8c, 0x0a7d32, 0xcf222e, 0x57606a] {
            let fixed = enforce_contrast(rgb(hex), bg, MIN_CONTRAST_RATIO);
            assert!(
                contrast_ratio(fixed, bg) >= MIN_CONTRAST_RATIO - 0.01,
                "{hex:#08x} reached only {:.2}",
                contrast_ratio(fixed, bg)
            );
        }
    }

    #[test]
    fn palette_polarity_follows_the_background() {
        assert!(is_dark_background(rgb(0x1e1e2e)));
        assert!(!is_dark_background(rgb(0xccd0da)));
        assert!(!is_dark_background(rgb(0xe8e8e8)));
    }

    #[test]
    fn every_theme_color_clears_the_target() {
        // Backgrounds from the shipped themes' alternate-base slots.
        for bg_hex in [0xccd0da, 0xe8e8e8, 0xe6e8eb, 0xb8c5db, 0xe8dccd, 0x313244, 0x44475a] {
            let bg = rgb(bg_hex);
            let theme = build_theme(bg);
            for item in &theme.scopes {
                let color = item.style.foreground.expect("scope has a foreground");
                assert!(
                    contrast_ratio(color, bg) >= MIN_CONTRAST_RATIO - 0.01,
                    "{bg_hex:#08x}: {color:?} reached only {:.2}",
                    contrast_ratio(color, bg)
                );
            }
        }
    }

    #[test]
    fn parse_background_accepts_qt_color_names() {
        assert_eq!(parse_background("#ccd0da"), rgb(0xccd0da));
        assert_eq!(parse_background("ccd0da"), rgb(0xccd0da));
        assert_eq!(parse_background(""), rgb(0xffffff));
        assert_eq!(parse_background("nonsense"), rgb(0xffffff));
    }
}
