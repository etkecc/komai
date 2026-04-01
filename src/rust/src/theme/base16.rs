// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Value};

use crate::ffi::{ThemeBase16PaletteData, ThemeBase16ParseResult};

pub fn parse_base16_yaml(theme_text: &str) -> ThemeBase16ParseResult {
    let root = match serde_yaml_ng::from_str::<Value>(theme_text) {
        Ok(value) => value,
        Err(error) => {
            return ThemeBase16ParseResult {
                has_document: false,
                error_message: error.to_string(),
                name: String::new(),
                author: String::new(),
                palette: empty_palette(),
            };
        }
    };

    let name = read_scalar_string(&root, "name");
    let author = read_scalar_string(&root, "author");
    let palette_source = palette_mapping(&root).unwrap_or_else(|| mapping_or_empty(&root));

    ThemeBase16ParseResult {
        has_document: true,
        error_message: String::new(),
        name,
        author,
        palette: ThemeBase16PaletteData {
            base00: read_base16_slot(&palette_source, "base00"),
            base01: read_base16_slot(&palette_source, "base01"),
            base02: read_base16_slot(&palette_source, "base02"),
            base03: read_base16_slot(&palette_source, "base03"),
            base04: read_base16_slot(&palette_source, "base04"),
            base05: read_base16_slot(&palette_source, "base05"),
            base06: read_base16_slot(&palette_source, "base06"),
            base07: read_base16_slot(&palette_source, "base07"),
            base08: read_base16_slot(&palette_source, "base08"),
            base09: read_base16_slot(&palette_source, "base09"),
            base0a: read_base16_slot(&palette_source, "base0A"),
            base0b: read_base16_slot(&palette_source, "base0B"),
            base0c: read_base16_slot(&palette_source, "base0C"),
            base0d: read_base16_slot(&palette_source, "base0D"),
            base0e: read_base16_slot(&palette_source, "base0E"),
            base0f: read_base16_slot(&palette_source, "base0F"),
        },
    }
}

fn empty_palette() -> ThemeBase16PaletteData {
    ThemeBase16PaletteData {
        base00: String::new(),
        base01: String::new(),
        base02: String::new(),
        base03: String::new(),
        base04: String::new(),
        base05: String::new(),
        base06: String::new(),
        base07: String::new(),
        base08: String::new(),
        base09: String::new(),
        base0a: String::new(),
        base0b: String::new(),
        base0c: String::new(),
        base0d: String::new(),
        base0e: String::new(),
        base0f: String::new(),
    }
}

fn mapping_or_empty(root: &Value) -> Mapping {
    match root {
        Value::Mapping(mapping) => mapping.clone(),
        _ => Mapping::new(),
    }
}

fn palette_mapping(root: &Value) -> Option<Mapping> {
    let Value::Mapping(mapping) = root else {
        return None;
    };

    match mapping.get(Value::String("palette".to_owned())) {
        Some(Value::Mapping(palette)) => Some(palette.clone()),
        _ => None,
    }
}

fn read_scalar_string(root: &Value, key: &str) -> String {
    let Value::Mapping(mapping) = root else {
        return String::new();
    };

    match mapping.get(Value::String(key.to_owned())) {
        Some(Value::String(value)) => value.clone(),
        _ => String::new(),
    }
}

fn read_base16_slot(mapping: &Mapping, key: &str) -> String {
    match mapping.get(Value::String(key.to_owned())) {
        Some(Value::String(value)) if !value.is_empty() => {
            if value.starts_with('#') {
                value.clone()
            } else {
                format!("#{value}")
            }
        }
        _ => String::new(),
    }
}
