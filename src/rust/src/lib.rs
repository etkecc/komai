// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// rustc 1.94+ trips a query-depth overflow when computing async layouts in
// the matrix-sdk timeline future graph (e.g. fetch_room_timeline).
// matrix-rust-sdk PR #6489 raises the limit, but `recursion_limit` is
// per-crate and applies to the crate currently being compiled — so the
// consumer has to repeat it.
#![recursion_limit = "256"]

pub mod composer_format;
pub mod composer_mentions;
pub mod composer_trigger;
pub mod emoji;
pub mod html_processor;
pub mod image_ops;
pub mod logging;
pub mod matrix_backend;
pub mod serverlist;
pub mod settings;
pub mod spellcheck;
pub mod syntax_highlight;
pub mod theme;
pub mod transcription;
pub(crate) mod ffi;
