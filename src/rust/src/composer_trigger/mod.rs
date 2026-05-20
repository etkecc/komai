// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Composer-side helpers for deciding when inline trigger characters
//! (`:`, `@`, `#`, `~`) should open the autocomplete picker.
//!
//! The QML composer asks `trigger_at_word_boundary` whether the position of
//! a freshly-typed trigger char sits at a word boundary. If yes, the picker
//! opens; if no (we're mid-word, e.g. inside `user@example.com`), the
//! keystroke is treated as literal text.

/// Returns true when the trigger character at `trigger_byte_pos` in `text`
/// is preceded by a word boundary — i.e. start of input, whitespace,
/// punctuation, an emoji, or any non-letter / non-digit / non-underscore
/// scalar value. Returns false when the preceding scalar value is a
/// Unicode letter, number, or underscore (we're inside a word).
///
/// `trigger_byte_pos` is the UTF-8 byte offset of the trigger char itself
/// inside `text`. Callers pass the position the keystroke *just landed*,
/// not the cursor position after.
///
/// The function tolerates an offset of 0 (start of input → boundary) and
/// any offset >= text.len() (treat as past end → boundary). If the offset
/// falls in the middle of a multi-byte UTF-8 sequence we fall back to
/// "boundary" to avoid panicking; the caller's offset conversion should
/// already be on a char boundary in practice.
pub fn trigger_at_word_boundary(text: &str, trigger_byte_pos: usize) -> bool {
    if trigger_byte_pos == 0 {
        return true;
    }
    let clamped = trigger_byte_pos.min(text.len());
    if !text.is_char_boundary(clamped) {
        return true;
    }
    match text[..clamped].chars().next_back() {
        None => true,
        Some(c) => !is_word_char(c),
    }
}

fn is_word_char(c: char) -> bool {
    c == '_' || c.is_alphanumeric()
}

#[cfg(test)]
mod tests;
