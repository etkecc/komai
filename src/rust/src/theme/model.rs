// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde::Deserialize;
use serde_yaml_ng::Value;

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExternalThemeFile {
    pub name: String,
    #[serde(default, rename = "author")]
    pub _author: Option<String>,
    pub variant: String,
    pub palette: ExternalThemePalette,
    #[serde(rename = "userColors")]
    pub user_colors: ExternalThemeUserColors,
    #[serde(default, rename = "source_base16")]
    pub _source_base16: Option<Value>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExternalThemePalette {
    pub window: String,
    #[serde(rename = "windowText")]
    pub window_text: String,
    pub base: String,
    #[serde(rename = "alternateBase")]
    pub alternate_base: String,
    pub text: String,
    #[serde(rename = "brightText")]
    pub bright_text: String,
    pub button: String,
    #[serde(rename = "buttonText")]
    pub button_text: String,
    pub light: String,
    pub mid: String,
    pub dark: String,
    pub highlight: String,
    #[serde(rename = "highlightedText")]
    pub highlighted_text: String,
    pub link: String,
    #[serde(rename = "toolTipBase")]
    pub tool_tip_base: String,
    #[serde(rename = "toolTipText")]
    pub tool_tip_text: String,
    pub attention: String,
    #[serde(rename = "attentionText")]
    pub attention_text: String,
    pub success: String,
    pub warning: String,
    pub error: String,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExternalThemeUserColors {
    #[serde(rename = "self")]
    pub self_slot: ExternalThemeUserColorSlot,
    pub others: Vec<ExternalThemeUserColorSlot>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExternalThemeUserColorSlot {
    pub background: String,
    #[serde(default)]
    pub text: String,
    #[serde(default, rename = "secondaryText")]
    pub secondary_text: String,
    #[serde(default)]
    pub link: String,
}

fn is_hex_color(value: &str) -> bool {
    value.len() == 7
        && value.starts_with('#')
        && value
            .as_bytes()
            .iter()
            .skip(1)
            .all(|byte| byte.is_ascii_hexdigit())
}

fn validate_required_color(path: &str, value: &str) -> Result<(), String> {
    if is_hex_color(value) {
        Ok(())
    } else {
        Err(format!("has invalid hex '{value}' for {path}"))
    }
}

fn validate_optional_color(path: &str, value: &str) -> Result<(), String> {
    if value.is_empty() || is_hex_color(value) {
        Ok(())
    } else {
        Err(format!("has invalid hex '{value}' for {path}"))
    }
}

impl ExternalThemeFile {
    pub fn validate(&self) -> Result<(), String> {
        if self.name.trim().is_empty() {
            return Err("missing or empty 'name'".to_owned());
        }

        if self.variant != "light" && self.variant != "dark" {
            return Err(format!(
                "has invalid variant '{}' (expected 'light' or 'dark')",
                self.variant
            ));
        }

        self.palette.validate()?;
        self.user_colors.validate()?;
        Ok(())
    }
}

impl ExternalThemePalette {
    pub fn validate(&self) -> Result<(), String> {
        validate_required_color("key 'window'", &self.window)?;
        validate_required_color("key 'windowText'", &self.window_text)?;
        validate_required_color("key 'base'", &self.base)?;
        validate_required_color("key 'alternateBase'", &self.alternate_base)?;
        validate_required_color("key 'text'", &self.text)?;
        validate_required_color("key 'brightText'", &self.bright_text)?;
        validate_required_color("key 'button'", &self.button)?;
        validate_required_color("key 'buttonText'", &self.button_text)?;
        validate_required_color("key 'light'", &self.light)?;
        validate_required_color("key 'mid'", &self.mid)?;
        validate_required_color("key 'dark'", &self.dark)?;
        validate_required_color("key 'highlight'", &self.highlight)?;
        validate_required_color("key 'highlightedText'", &self.highlighted_text)?;
        validate_required_color("key 'link'", &self.link)?;
        validate_required_color("key 'toolTipBase'", &self.tool_tip_base)?;
        validate_required_color("key 'toolTipText'", &self.tool_tip_text)?;
        validate_required_color("key 'attention'", &self.attention)?;
        validate_required_color("key 'attentionText'", &self.attention_text)?;
        validate_required_color("key 'success'", &self.success)?;
        validate_required_color("key 'warning'", &self.warning)?;
        validate_required_color("key 'error'", &self.error)?;
        Ok(())
    }
}

impl ExternalThemeUserColors {
    pub fn validate(&self) -> Result<(), String> {
        self.self_slot.validate("userColors.self")?;

        if self.others.is_empty() {
            return Err("userColors.others must have at least 1 entry".to_owned());
        }

        for (index, slot) in self.others.iter().enumerate() {
            slot.validate(&format!("userColors.others[{index}]"))?;
        }

        Ok(())
    }
}

impl ExternalThemeUserColorSlot {
    pub fn validate(&self, path: &str) -> Result<(), String> {
        validate_required_color(&format!("{path}.background"), &self.background)?;
        validate_optional_color(&format!("{path}.text"), &self.text)?;
        validate_optional_color(&format!("{path}.secondaryText"), &self.secondary_text)?;
        validate_optional_color(&format!("{path}.link"), &self.link)?;
        Ok(())
    }
}
