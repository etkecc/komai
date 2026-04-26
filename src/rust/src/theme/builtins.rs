// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi::{
    ThemeBuiltinEntry, ThemeBuiltinListResult, ThemeExternalDefinition, ThemePaletteData,
    ThemeUserColorSlotData,
};

use super::model::{ExternalThemeFile, ExternalThemePalette, ExternalThemeUserColorSlot};

const BUILTIN_THEMES: &[(&str, i32, &str)] = &[
    (
        "light-komai",
        0,
        include_str!("../../../../resources/themes/light-komai.yml"),
    ),
    (
        "dark-komai",
        1,
        include_str!("../../../../resources/themes/dark-komai.yml"),
    ),
    (
        "light-catppuccin-latte",
        200,
        include_str!("../../../../resources/themes/light-catppuccin-latte.yml"),
    ),
    (
        "dark-catppuccin-mocha",
        200,
        include_str!("../../../../resources/themes/dark-catppuccin-mocha.yml"),
    ),
    (
        "dark-dracula",
        200,
        include_str!("../../../../resources/themes/dark-dracula.yml"),
    ),
    (
        "dark-matrix",
        200,
        include_str!("../../../../resources/themes/dark-matrix.yml"),
    ),
    (
        "light-nord",
        200,
        include_str!("../../../../resources/themes/light-nord.yml"),
    ),
    (
        "dark-nord",
        200,
        include_str!("../../../../resources/themes/dark-nord.yml"),
    ),
    (
        "light-rose-pine-dawn",
        200,
        include_str!("../../../../resources/themes/light-rose-pine-dawn.yml"),
    ),
    (
        "dark-rose-pine-moon",
        200,
        include_str!("../../../../resources/themes/dark-rose-pine-moon.yml"),
    ),
    (
        "dark-tokyo-night",
        200,
        include_str!("../../../../resources/themes/dark-tokyo-night.yml"),
    ),
    (
        "light-nheko",
        100,
        include_str!("../../../../resources/themes/light-nheko.yml"),
    ),
    (
        "dark-nheko",
        100,
        include_str!("../../../../resources/themes/dark-nheko.yml"),
    ),
];

pub fn builtin_themes() -> ThemeBuiltinListResult {
    let mut themes = Vec::with_capacity(BUILTIN_THEMES.len());
    let mut errors = Vec::new();

    for &(slug, sort_order, yaml_text) in BUILTIN_THEMES {
        match serde_yaml_ng::from_str::<ExternalThemeFile>(yaml_text) {
            Ok(parsed) => {
                if let Err(error) = parsed.validate() {
                    errors.push(format!("{slug}: {error}"));
                    continue;
                }
                themes.push(ThemeBuiltinEntry {
                    slug: slug.to_owned(),
                    sort_order,
                    theme: to_ffi_definition(&parsed),
                });
            }
            Err(error) => {
                errors.push(format!("{slug}: {error}"));
            }
        }
    }

    ThemeBuiltinListResult { themes, errors }
}

fn to_ffi_definition(parsed: &ExternalThemeFile) -> ThemeExternalDefinition {
    ThemeExternalDefinition {
        name: parsed.name.clone(),
        variant: parsed.variant.clone(),
        palette: palette_to_ffi(&parsed.palette),
        user_color_self: slot_to_ffi(&parsed.user_colors.self_slot),
        user_color_others: parsed.user_colors.others.iter().map(slot_to_ffi).collect(),
    }
}

fn palette_to_ffi(palette: &ExternalThemePalette) -> ThemePaletteData {
    ThemePaletteData {
        window: palette.window.clone(),
        window_text: palette.window_text.clone(),
        base: palette.base.clone(),
        alternate_base: palette.alternate_base.clone(),
        text: palette.text.clone(),
        bright_text: palette.bright_text.clone(),
        button: palette.button.clone(),
        button_text: palette.button_text.clone(),
        light: palette.light.clone(),
        mid: palette.mid.clone(),
        dark: palette.dark.clone(),
        highlight: palette.highlight.clone(),
        highlighted_text: palette.highlighted_text.clone(),
        link: palette.link.clone(),
        tool_tip_base: palette.tool_tip_base.clone(),
        tool_tip_text: palette.tool_tip_text.clone(),
        attention: palette.attention.clone(),
        attention_text: palette.attention_text.clone(),
        success: palette.success.clone(),
        warning: palette.warning.clone(),
        error: palette.error.clone(),
    }
}

fn slot_to_ffi(slot: &ExternalThemeUserColorSlot) -> ThemeUserColorSlotData {
    ThemeUserColorSlotData {
        background: slot.background.clone(),
        text: slot.text.clone(),
        secondary_text: slot.secondary_text.clone(),
        link: slot.link.clone(),
    }
}
