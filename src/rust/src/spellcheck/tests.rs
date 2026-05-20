// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

#[test]
fn normalize_code_variants() {
    assert_eq!(normalize_code("en_US"), "en_US");
    assert_eq!(normalize_code("en-US"), "en_US");
    assert_eq!(normalize_code("en_GB-large"), "en_GB");
    assert_eq!(normalize_code("en_GB-ize"), "en_GB");
    assert_eq!(normalize_code("bg_BG"), "bg_BG");
    assert_eq!(normalize_code("de"), "de");
    assert_eq!(normalize_code(""), "");
    assert_eq!(normalize_code("123"), "");
}

#[test]
fn word_spans_basic() {
    let t = "Hello, world! It's a test—really.";
    let spans: Vec<&str> = word_spans(t).into_iter().map(|(s, e)| &t[s..e]).collect();
    assert_eq!(spans, vec!["Hello", "world", "It's", "a", "test", "really"]);
}

#[test]
fn looks_checkable_filters() {
    assert!(looks_checkable("hello"));
    assert!(looks_checkable("It's"));
    assert!(looks_checkable("Hello"));
    assert!(!looks_checkable("a")); // too short
    assert!(!looks_checkable("HTTP")); // all caps
    assert!(!looks_checkable("getElementById")); // camelCase
    assert!(!looks_checkable("iPhone")); // inner caps
    assert!(!looks_checkable("h2o")); // has a digit
}

#[test]
fn looks_checkable_rejects_fullwidth_forms() {
    // Zenkaku (fullwidth Latin) is what a Japanese IME emits in "wide Latin"
    // mode — Unicode marks it Script=Latin, but it isn't an English word.
    assert!(!looks_checkable("ｈｅｌｌｏ")); // fullwidth lowercase
    assert!(!looks_checkable("Ｈｅｌｌｏ")); // fullwidth title-case
    assert!(!looks_checkable("ｈello")); // mixed — still CJK-context
    assert!(looks_checkable("hello")); // regression: normal ASCII still checks
}

#[test]
fn skip_ranges_catch_urls_mentions_code() {
    let t = "see https://example.org and @alice:server and `code` and :smile:";
    let r = skip_ranges(t);
    // The word "example" inside the URL must be covered.
    let idx = t.find("example").unwrap();
    assert!(overlaps_any(idx, idx + 7, &r));
    // "alice" inside the mention.
    let idx = t.find("alice").unwrap();
    assert!(overlaps_any(idx, idx + 5, &r));
    // "code" inside the backticks.
    let idx = t.find("code").unwrap();
    assert!(overlaps_any(idx, idx + 4, &r));
    // "smile" inside the shortcode.
    let idx = t.find("smile").unwrap();
    assert!(overlaps_any(idx, idx + 5, &r));
}

#[test]
fn bundled_en_us_dictionary_parses_and_checks() {
    // The bundled fallback dictionary must actually load with spellbook and
    // give sane answers — otherwise the composer would silently flag
    // nothing.
    let aff = include_str!("../../../../resources/dictionaries/en_US.aff");
    let dic = include_str!("../../../../resources/dictionaries/en_US.dic");
    let dict = Dictionary::new(aff, dic).expect("bundled en_US dictionary must parse");
    assert!(dict.check("hello"));
    assert!(dict.check("Hello"));
    assert!(dict.check("computer"));
    assert!(!dict.check("helllo"));
    assert!(!dict.check("teh"));
    let mut sugg = Vec::new();
    dict.suggest("helllo", &mut sugg);
    assert!(sugg.iter().any(|s| s == "hello"), "expected 'hello' among {sugg:?}");
}

#[test]
fn scripts_basics() {
    assert_eq!(scripts_of_word("hello"), vec![Script::Latin]);
    assert_eq!(scripts_of_word("don't"), vec![Script::Latin]);
    assert_eq!(scripts_of_word("здравей"), vec![Script::Cyrillic]);
    assert!(script_compatible(&[Script::Latin], &[Script::Latin]));
    assert!(!script_compatible(&[Script::Latin], &[Script::Cyrillic]));
    assert!(script_compatible(&[], &[Script::Cyrillic])); // unknown dict
    assert_eq!(scripts_for_locale("en_US"), vec![Script::Latin]);
    assert_eq!(scripts_for_locale("bg_BG"), vec![Script::Cyrillic]);
}
