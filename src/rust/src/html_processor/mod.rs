// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! HTML processing pipeline for Matrix timeline messages.
//!
//! Ports the C++ `HtmlProcessor` (sanitize, linkify, pill decoration,
//! plain-text-to-HTML) to Rust so processing happens before the FFI boundary.

mod format;
mod linkify;
pub(crate) mod parser;
mod pills;
mod sanitize;
mod search;
mod util;

pub(crate) use format::format_body_html;
pub(crate) use linkify::linkify_html;
pub(crate) use sanitize::sanitize_html;
pub(crate) use search::mark_search_matches;
