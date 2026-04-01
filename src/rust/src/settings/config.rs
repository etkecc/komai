// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::Value;

#[derive(Clone, Debug, Default)]
pub struct StoredConfig {
    pub ui_scale_factor: Option<f32>,
}

pub fn parse_config_text(config_text: &str) -> StoredConfig {
    let root = match serde_yaml_ng::from_str::<Value>(config_text) {
        Ok(root) => root,
        Err(_) => return StoredConfig::default(),
    };

    StoredConfig {
        ui_scale_factor: value_at_path(&root, &["ui", "scale", "factor"])
            .and_then(parse_scalar_f32)
            .and_then(normalize_scale_factor),
    }
}

fn value_at_path<'a>(root: &'a Value, path: &[&str]) -> Option<&'a Value> {
    let mut current = root;

    for key in path {
        current = match current {
            Value::Mapping(map) => map.get(Value::String((*key).to_owned()))?,
            _ => return None,
        };
    }

    Some(current)
}

fn parse_scalar_f32(value: &Value) -> Option<f32> {
    match value {
        Value::Number(number) => number.as_f64().map(|value| value as f32),
        Value::String(value) => value.trim().parse::<f32>().ok(),
        _ => None,
    }
}

fn normalize_scale_factor(factor: f32) -> Option<f32> {
    (1.0..=3.0).contains(&factor).then_some(factor)
}

#[cfg(test)]
mod tests {
    use super::parse_config_text;

    #[test]
    fn parses_valid_scale_factor() {
        let config = parse_config_text(
            r#"
ui:
  scale:
    factor: 1.75
"#,
        );

        assert_eq!(config.ui_scale_factor, Some(1.75));
    }

    #[test]
    fn ignores_out_of_range_scale_factor() {
        let config = parse_config_text(
            r#"
ui:
  scale:
    factor: 5
"#,
        );

        assert_eq!(config.ui_scale_factor, None);
    }

    #[test]
    fn ignores_malformed_scale_factor() {
        let config = parse_config_text(
            r#"
ui:
  scale:
    factor: nope
"#,
        );

        assert_eq!(config.ui_scale_factor, None);
    }
}
