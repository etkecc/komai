// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Resolve which syntect syntax to use for a fenced code block: parse
//! the `language-foo` class attribute, normalize to syntect names, and
//! fall back to content sniffing.

use super::*;

// ---------------------------------------------------------------------------
// Language detection & resolution
// ---------------------------------------------------------------------------

pub(super) fn extract_language_token(code_attrs: &str) -> String {
    if code_attrs.is_empty() || code_attrs.len() > MAX_ATTRIBUTE_CHARS {
        return String::new();
    }

    let bytes = code_attrs.as_bytes();
    let mut cursor = 0;

    while cursor < bytes.len() {
        // Skip whitespace.
        while cursor < bytes.len() && bytes[cursor].is_ascii_whitespace() {
            cursor += 1;
        }
        if cursor >= bytes.len() {
            break;
        }

        // Read attribute name.
        let name_start = cursor;
        while cursor < bytes.len()
            && !bytes[cursor].is_ascii_whitespace()
            && bytes[cursor] != b'='
        {
            cursor += 1;
        }
        if cursor == name_start {
            break;
        }
        let name = &code_attrs[name_start..cursor];

        // Skip whitespace.
        while cursor < bytes.len() && bytes[cursor].is_ascii_whitespace() {
            cursor += 1;
        }

        let mut value = String::new();
        if cursor < bytes.len() && bytes[cursor] == b'=' {
            cursor += 1;
            while cursor < bytes.len() && bytes[cursor].is_ascii_whitespace() {
                cursor += 1;
            }

            if cursor < bytes.len() && (bytes[cursor] == b'"' || bytes[cursor] == b'\'') {
                let quote = bytes[cursor];
                cursor += 1;
                let val_start = cursor;
                while cursor < bytes.len() && bytes[cursor] != quote {
                    cursor += 1;
                }
                value = code_attrs[val_start..cursor].to_owned();
                if cursor < bytes.len() {
                    cursor += 1;
                }
            } else {
                let val_start = cursor;
                while cursor < bytes.len() && !bytes[cursor].is_ascii_whitespace() {
                    cursor += 1;
                }
                value = code_attrs[val_start..cursor].to_owned();
            }
        }

        if name.eq_ignore_ascii_case("class") {
            return extract_language_from_class_value(&value);
        }
    }

    String::new()
}

pub(super) fn extract_language_from_class_value(class_value: &str) -> String {
    for token in class_value.split_whitespace() {
        let lang = token.to_ascii_lowercase();
        let lang = if let Some(rest) = lang.strip_prefix("language-") {
            rest
        } else if let Some(rest) = lang.strip_prefix("lang-") {
            rest
        } else {
            continue;
        };

        if lang.is_empty() || lang.len() > MAX_LANGUAGE_TOKEN_CHARS {
            continue;
        }

        let valid = lang
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b == b'+' || b == b'#' || b == b'-' || b == b'_' || b == b'.');
        if valid {
            return lang.to_owned();
        }
    }
    String::new()
}

pub(super) fn resolve_syntax<'a>(ss: &'a SyntaxSet, token: &str) -> Option<&'a SyntaxReference> {
    if token.is_empty() {
        return None;
    }

    let lowered = token.to_ascii_lowercase();

    // Try by name (case-insensitive scan).
    if let Some(s) = ss.syntaxes().iter().find(|s| s.name.eq_ignore_ascii_case(token)) {
        return Some(s);
    }

    // Alias mapping.
    let extension = match lowered.as_str() {
        "c++" => "cpp",
        "c#" => "cs",
        "shell" | "shell-session" => "sh",
        "yml" => "yaml",
        _ => &lowered,
    };

    // Try by extension.
    if let Some(s) = ss.find_syntax_by_extension(extension) {
        return Some(s);
    }

    // Try lowered name again.
    if let Some(s) = ss.syntaxes().iter().find(|s| s.name.eq_ignore_ascii_case(&lowered)) {
        return Some(s);
    }

    // Special diff/patch handling.
    if lowered == "diff" || lowered == "patch" {
        if let Some(s) = ss.find_syntax_by_extension("diff") {
            return Some(s);
        }
        // Also try by name "Diff".
        return ss.syntaxes().iter().find(|s| s.name == "Diff");
    }

    None
}

pub(super) fn detect_syntax_from_content<'a>(ss: &'a SyntaxSet, code: &str) -> Option<&'a SyntaxReference> {
    let token = detect_language_token(code);
    if !token.is_empty() {
        return resolve_syntax(ss, &token);
    }
    None
}

pub(super) fn detect_language_token(code: &str) -> String {
    let trimmed = code.trim();
    if trimmed.is_empty() {
        return String::new();
    }

    if trimmed.starts_with("<?php") {
        return "php".to_owned();
    }

    if trimmed.starts_with("#!/") {
        let first_line = trimmed.lines().next().unwrap_or("").to_ascii_lowercase();
        if first_line.contains("python") {
            return "python".to_owned();
        }
        if first_line.contains("bash") || first_line.contains("zsh") || first_line.contains("/sh")
        {
            return "bash".to_owned();
        }
        if first_line.contains("node") {
            return "javascript".to_owned();
        }
    }

    // Diff detection.
    for line in trimmed.lines() {
        if line.starts_with("diff --git")
            || line.starts_with("@@ ")
            || line.starts_with("--- ")
            || line.starts_with("+++ ")
        {
            return "diff".to_owned();
        }
    }

    // JSON detection.
    if trimmed.starts_with('{') || trimmed.starts_with('[') {
        if serde_json::from_str::<serde_json::Value>(trimmed).is_ok() {
            return "json".to_owned();
        }
    }

    // XML detection.
    if trimmed.starts_with("<?xml")
        || (trimmed.starts_with('<') && trimmed.ends_with('>'))
    {
        return "xml".to_owned();
    }

    String::new()
}
