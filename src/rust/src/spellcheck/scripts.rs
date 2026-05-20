// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Unicode-script detection for words and dictionaries, plus locale-code
//! normalization to the `xx_YY` form spellbook expects.

use super::*;

pub(super) fn scripts_of_word(word: &str) -> Vec<Script> {
    let mut v: Vec<Script> = Vec::new();
    for c in word.chars() {
        let s = c.script();
        if matches!(s, Script::Common | Script::Inherited | Script::Unknown) {
            continue;
        }
        if !v.contains(&s) {
            v.push(s);
        }
    }
    v
}

pub(super) fn scripts_for_locale(locale_code: &str) -> Vec<Script> {
    let lang = locale_code
        .split(|c| c == '_' || c == '-')
        .next()
        .unwrap_or("")
        .to_ascii_lowercase();
    let s = match lang.as_str() {
        // Cyrillic
        "ru" | "uk" | "be" | "bg" | "mk" | "sr" | "ky" | "kk" | "tg" | "mn" => Some(Script::Cyrillic),
        // Greek
        "el" => Some(Script::Greek),
        // Hebrew / Yiddish
        "he" | "yi" => Some(Script::Hebrew),
        // Arabic-script languages
        "ar" | "fa" | "ur" | "ps" | "ckb" | "sd" => Some(Script::Arabic),
        "hy" => Some(Script::Armenian),
        "ka" => Some(Script::Georgian),
        "am" | "ti" => Some(Script::Ethiopic),
        "hi" | "mr" | "ne" | "sa" => Some(Script::Devanagari),
        "bn" => Some(Script::Bengali),
        "ta" => Some(Script::Tamil),
        "th" => Some(Script::Thai),
        "ko" => Some(Script::Hangul),
        // Everything else with a Hunspell dictionary is Latin in practice
        // (en, de, fr, es, it, pt, nl, pl, cs, sk, hu, ro, tr, vi, …).
        "" => None,
        _ => Some(Script::Latin),
    };
    s.into_iter().collect()
}

pub(super) fn script_compatible(dict_scripts: &[Script], word_scripts: &[Script]) -> bool {
    if dict_scripts.is_empty() || word_scripts.is_empty() {
        return true;
    }
    word_scripts.iter().any(|w| dict_scripts.contains(w))
}

pub(super) fn normalize_code(raw: &str) -> String {
    let raw = raw.trim();
    if raw.is_empty() {
        return String::new();
    }
    let is_sep = |c: char| c == '_' || c == '-';
    // First separator (whichever of '_' / '-' appears first) is the
    // language/region boundary; everything from the next separator on is a
    // suffix we don't care about.
    let (lang, rest) = match raw.find(is_sep) {
        Some(i) => (&raw[..i], Some(&raw[i + 1..])),
        None => (raw, None),
    };
    let lang = lang.to_ascii_lowercase();
    if lang.is_empty() || !lang.chars().all(|c| c.is_ascii_alphabetic()) {
        return String::new();
    }
    match rest {
        Some(rest) => {
            let region: &str = match rest.find(is_sep) {
                Some(i) => &rest[..i],
                None => rest,
            };
            if region.is_empty() || !region.chars().all(|c| c.is_ascii_alphanumeric()) {
                lang
            } else {
                format!("{lang}_{}", region.to_ascii_uppercase())
            }
        }
        None => lang,
    }
}
