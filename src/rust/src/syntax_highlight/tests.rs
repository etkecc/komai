// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

/// A dark code surface (Catppuccin Mocha's alternate base), standing in for the
/// palette the timeline passes at runtime.
const DARK_BG: &str = "#313244";

/// Collect the `#rrggbb` values from every `style="color:…"` the highlighter
/// emitted, so a test can check them against the background they'll sit on.
fn inline_colors(html: &str) -> Vec<syntect::highlighting::Color> {
    let mut colors = Vec::new();
    let mut rest = html;
    while let Some(offset) = rest.find("color:#") {
        rest = &rest[offset + "color:#".len()..];
        if rest.len() >= 6 {
            colors.push(palette::parse_background(&rest[..6]));
        }
    }
    colors
}

#[test]
fn highlights_recognized_language() {
    let input = r#"<pre><code class="language-diff">+ line
- line</code></pre>"#;
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert!(
        output.contains("<span style=\""),
        "expected highlighted spans in output: {output}"
    );
    assert!(
        output.contains("<pre><code class=\"language-diff\">"),
        "should preserve original code tag attrs"
    );
}

#[test]
fn no_nested_pre_after_highlighting() {
    // Regression for https://github.com/etkecc/komai/issues/155 — syntect's
    // output `<pre style="…">…</pre>` was being spliced inside our own
    // `<pre><code>…</code></pre>`, producing two stacked `<pre>` boxes in
    // the rendered bubble (a faded outer rectangle and a prominent inner
    // one) because `strip_syntect_wrapper` looked for an inner `<code>`
    // tag that syntect never emits.
    let input = "<pre><code class=\"language-yaml\">a\nb\n</code></pre>";
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    let pre_opens = output.matches("<pre").count();
    let pre_closes = output.matches("</pre>").count();
    assert_eq!(
        pre_opens, 1,
        "exactly one <pre> opening tag expected, got: {output}"
    );
    assert_eq!(
        pre_closes, 1,
        "exactly one </pre> closing tag expected, got: {output}"
    );
    // syntect's inline background-color must not leak through.
    assert!(
        !output.contains("background-color"),
        "syntect background wrapper not stripped: {output}"
    );
}

#[test]
fn strip_syntect_wrapper_handles_pre_with_no_inner_code() {
    let input = "<pre style=\"background-color:#2b303b;\">\n<span>foo</span></pre>\n";
    assert_eq!(strip_syntect_wrapper(input), "<span>foo</span>");
}

#[test]
fn strip_syntect_wrapper_returns_original_when_no_pre() {
    assert_eq!(strip_syntect_wrapper("plain"), "plain");
}

#[test]
fn unknown_language_leaves_block_unchanged() {
    let input = r#"<pre><code class="language-komai-unknown">hello()</code></pre>"#;
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert_eq!(output, input);
}

#[test]
fn disabled_returns_original_html() {
    // An empty string triggers the early return, but let's test the real
    // disabled path by passing through normally — the C++ caller gates on
    // the setting.  For the Rust function, absence of the call *is* the
    // disabled path.  Still, verify passthrough for oversized input.
    let large = "x".repeat(MAX_HTML_CHARS + 1);
    let output = highlight_formatted_code_blocks(&large, DARK_BG);
    assert_eq!(output, large);
}

#[test]
fn parses_language_class_case_insensitively() {
    let input = r#"<pre><code CLASS='LANGUAGE-PHP'>&lt;?php
echo "Hello, world!";
</code></pre>"#;
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert!(output.contains("<span style=\""));
}

#[test]
fn auto_detects_php() {
    let input = r#"<pre><code>&lt;?php
echo "Hello, world!";
</code></pre>"#;
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert!(
        output.contains("<span style=\""),
        "should auto-detect PHP from content"
    );
}

#[test]
fn allows_whitespace_between_pre_and_code() {
    let input =
        "<pre>\n  <code class=\"language-json\">{\"hello\": \"world\"}</code>\n</pre>";
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert!(
        output.contains("<span style=\""),
        "whitespace between pre/code should still allow highlighting"
    );
}

#[test]
fn ignores_non_whitespace_between_pre_and_code() {
    let input = r#"<pre><span>prefix</span><code class="language-json">{"hello": "world"}</code></pre>"#;
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert_eq!(output, input);
}

#[test]
fn no_language_plain_text_unchanged() {
    let input = "<pre><code>plain text</code></pre>";
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert_eq!(output, input);
}

#[test]
fn malformed_code_block_unchanged() {
    let input = r#"<pre><code class="language-cpp">int x = 1;"#;
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert_eq!(output, input);
}

#[test]
fn large_code_block_unchanged() {
    let large_payload = "x".repeat(25000);
    let input = format!(
        r#"<pre><code class="language-cpp">{large_payload}</code></pre>"#
    );
    let output = highlight_formatted_code_blocks(&input, DARK_BG);
    assert_eq!(output, input);
}

#[test]
fn oversized_html_unchanged() {
    let prefix = "x".repeat(1_000_005);
    let input = format!(
        r#"{prefix}<pre><code class="language-cpp">int x = 1;</code></pre>"#
    );
    let output = highlight_formatted_code_blocks(&input, DARK_BG);
    assert_eq!(output, input);
}

#[test]
fn highlighted_code_escapes_html_special_chars() {
    let input =
        r#"<pre><code class="language-html">&lt;img src=x onerror=alert(1)&gt;</code></pre>"#;
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert!(
        !output.contains("<img"),
        "output must not contain unescaped HTML tags from code: {output}"
    );
}

#[test]
fn highlight_raw_json_works() {
    let json = r#"{"hello": "world"}"#;
    let output = highlight_raw_json(json, DARK_BG);
    assert!(output.contains("<span style=\""));
    assert!(output.contains("<pre><code class=\"language-json\">"));
}

#[test]
fn highlight_raw_json_preserves_emoji() {
    let json = r#"{"body": "🤷‍♂️"}"#;
    let output = highlight_raw_json(json, DARK_BG);
    assert!(
        output.contains("🤷‍♂️"),
        "emoji should be preserved in highlighted output: {output}"
    );
}

#[test]
fn decode_html_entities_preserves_emoji() {
    // Emojis should pass through decode_html_entities unchanged.
    assert_eq!(decode_html_entities("🤷‍♂️"), "🤷‍♂️");
    assert_eq!(decode_html_entities("hi 🎉 &amp; bye"), "hi 🎉 & bye");
}

#[test]
fn preserves_newlines_in_highlighted_code() {
    let input = r#"<pre><code class="language-php">&lt;?php
class Greeter {
public function hello(): string {
    return "Hello, world!";
}
}
</code></pre><p>Hey!</p>"#;
    let output = highlight_formatted_code_blocks(input, DARK_BG);
    assert!(output.contains("<span style=\""));
    // Verify the trailing <p>Hey!</p> is preserved.
    assert!(output.contains("<p>Hey!</p>"));
}

#[test]
fn decode_html_entities_works() {
    assert_eq!(decode_html_entities("&lt;div&gt;"), "<div>");
    assert_eq!(decode_html_entities("&amp;"), "&");
    assert_eq!(decode_html_entities("&#60;"), "<");
    assert_eq!(decode_html_entities("&#x3C;"), "<");
    assert_eq!(decode_html_entities("no entities"), "no entities");
}

#[test]
fn extract_language_token_works() {
    assert_eq!(
        extract_language_token(" class=\"language-python\""),
        "python"
    );
    assert_eq!(extract_language_token(" class='lang-rust'"), "rust");
    assert_eq!(extract_language_token(" class=\"foo bar\""), "");
    assert_eq!(extract_language_token(""), "");
}

#[test]
fn caps_at_block_limit() {
    let mut input = String::new();
    for i in 0..70 {
        input.push_str(&format!(
            r#"<pre><code class="language-json">{{"k": {i}}}</code></pre>"#
        ));
    }
    let output = highlight_formatted_code_blocks(&input, DARK_BG);
    let highlighted_count = output.matches("<span style=\"").count();
    // Each highlighted JSON block gets at least one span. With 64-block limit,
    // we should not exceed that.
    assert!(
        highlighted_count > 0,
        "should highlight at least some blocks"
    );
    // Count how many blocks got the highlight wrapper vs original
    let original_blocks_remaining = output
        .matches(r##"class="language-json">{"k":"##)
        .count();
    // At most 64 should be highlighted, the rest left as-is.
    assert!(
        original_blocks_remaining >= 6,
        "at least 6 blocks should be left unhighlighted (70 - 64 max)"
    );
}

#[test]
fn strip_syntect_wrapper_works() {
    // Matches the real shape of `syntect::html::highlighted_html_for_string`
    // output: a single `<pre style="background-color:#xxx;">` wrapper
    // around `<span>` runs, with a literal newline immediately after the
    // open tag and (sometimes) before the close tag. There is no inner
    // `<code>` element.
    let input = "<pre style=\"background-color:#2b303b;\">\n\
                 <span style=\"color:#a3be8c;\">hello</span></pre>\n";
    assert_eq!(
        strip_syntect_wrapper(input),
        "<span style=\"color:#a3be8c;\">hello</span>"
    );
}

#[test]
fn highlighted_tokens_stay_legible_on_theme_backgrounds() {
    // The JSON from issue #258, where syntect's light theme rendered keys in a
    // pastel green that all but vanished into the message bubble.
    let input = "<pre><code class=\"language-json\">{\n  \
                 &quot;cc.etke.ketesa&quot;: {\n    \
                 &quot;externalAuthProvider&quot;: true\n  }\n}</code></pre>";

    // Alternate-base slots of the shipped themes, light and dark.
    for background in [
        "#e8e8e8", // Komai Light
        "#ccd0da", // Catppuccin Latte
        "#e6e8eb", // Nheko Light
        "#b8c5db", // Nord Light
        "#e8dccd", // Rosé Pine Dawn
        "#313244", // Catppuccin Mocha
        "#44475a", // Dracula
    ] {
        let output = highlight_formatted_code_blocks(input, background);
        let surface = palette::parse_background(background);
        let colors = inline_colors(&output);
        assert!(
            !colors.is_empty(),
            "{background}: expected highlighted spans in {output}"
        );
        for color in colors {
            let ratio = palette::contrast_ratio(color, surface);
            assert!(
                ratio >= palette::MIN_CONTRAST_RATIO - 0.01,
                "{background}: {color:?} reached only {ratio:.2}"
            );
        }
    }
}
