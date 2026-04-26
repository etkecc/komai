// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi::{
    ThemeExternalDefinition, ThemeExternalParseResult, ThemePaletteData, ThemeUserColorSlotData,
};

use super::model::{ExternalThemeFile, ExternalThemePalette, ExternalThemeUserColorSlot};

pub fn parse_external_theme(theme_text: &str) -> ThemeExternalParseResult {
    let parsed = match serde_yaml_ng::from_str::<ExternalThemeFile>(theme_text) {
        Ok(value) => value,
        Err(error) => return error_result(error.to_string()),
    };

    if let Err(error) = parsed.validate() {
        return error_result(error);
    }

    ThemeExternalParseResult {
        has_theme: true,
        error_message: String::new(),
        theme: ThemeExternalDefinition {
            name: parsed.name,
            variant: parsed.variant,
            palette: palette_to_ffi(&parsed.palette),
            user_color_self: slot_to_ffi(&parsed.user_colors.self_slot),
            user_color_others: parsed.user_colors.others.iter().map(slot_to_ffi).collect(),
        },
    }
}

fn error_result(message: String) -> ThemeExternalParseResult {
    ThemeExternalParseResult {
        has_theme: false,
        error_message: message,
        theme: ThemeExternalDefinition {
            name: String::new(),
            variant: String::new(),
            palette: ThemePaletteData {
                window: String::new(),
                window_text: String::new(),
                base: String::new(),
                alternate_base: String::new(),
                text: String::new(),
                bright_text: String::new(),
                button: String::new(),
                button_text: String::new(),
                light: String::new(),
                mid: String::new(),
                dark: String::new(),
                highlight: String::new(),
                highlighted_text: String::new(),
                link: String::new(),
                tool_tip_base: String::new(),
                tool_tip_text: String::new(),
                attention: String::new(),
                attention_text: String::new(),
                success: String::new(),
                warning: String::new(),
                error: String::new(),
            },
            user_color_self: ThemeUserColorSlotData {
                background: String::new(),
                text: String::new(),
                secondary_text: String::new(),
                link: String::new(),
            },
            user_color_others: Vec::new(),
        },
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
