// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    io::{self, Write},
    sync::OnceLock,
};

use tracing_log::LogTracer;
use tracing_subscriber::{
    fmt,
    layer::SubscriberExt,
    util::SubscriberInitExt,
    EnvFilter,
};

static INIT: OnceLock<()> = OnceLock::new();

/// Cap on the formatted byte size of a single log event written to stderr.
/// Overridable via `KOMAI_LOG_MAX_EVENT_BYTES` (`0` disables the cap).
///
/// Some dependencies attach entire data structures to log events —
/// matrix-sdk-ui's timeline invariant checks (`duplicate read receipts in
/// this timeline`) include a Debug dump of every item in the timeline,
/// producing single log lines of hundreds of kilobytes that flood the
/// terminal. The head of such an event carries the signal; the tail is
/// noise.
const DEFAULT_MAX_EVENT_BYTES: usize = 16 * 1024;

fn max_event_bytes() -> Option<usize> {
    static VALUE: OnceLock<Option<usize>> = OnceLock::new();
    *VALUE.get_or_init(|| {
        match std::env::var("KOMAI_LOG_MAX_EVENT_BYTES") {
            Ok(value) => match value.trim().parse::<usize>() {
                Ok(0) => None,
                Ok(limit) => Some(limit),
                Err(_) => Some(DEFAULT_MAX_EVENT_BYTES),
            },
            Err(_) => Some(DEFAULT_MAX_EVENT_BYTES),
        }
    })
}

/// An [`io::Write`] wrapper that passes through at most `limit` bytes and
/// swallows the rest, emitting a truncation marker once.
///
/// The fmt layer requests one writer per log event, so the byte budget is
/// per event.
struct TruncatingWriter<W: Write> {
    inner: W,
    limit: usize,
    written: usize,
}

impl<W: Write> TruncatingWriter<W> {
    fn new(inner: W, limit: usize) -> Self {
        Self { inner, limit, written: 0 }
    }
}

impl<W: Write> Write for TruncatingWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if self.written > self.limit {
            // Already truncated: swallow the rest of the event.
            return Ok(buf.len());
        }

        let remaining = self.limit - self.written;
        if buf.len() <= remaining {
            self.written += buf.len();
            self.inner.write_all(buf)?;
            return Ok(buf.len());
        }

        // Cut on a UTF-8 character boundary so the terminal doesn't
        // receive a broken multi-byte sequence.
        let mut cut = remaining;
        while cut > 0 && (buf[cut] & 0xC0) == 0x80 {
            cut -= 1;
        }

        self.inner.write_all(&buf[..cut])?;
        // Reset ANSI state first: the cut may land inside colored output.
        self.inner.write_all(
            b"\x1b[0m ... [log event truncated; set KOMAI_LOG_MAX_EVENT_BYTES=0 to disable]\n",
        )?;
        // Mark as truncated (strictly past the limit) so later writes for
        // this event — including its trailing newline — are swallowed.
        self.written = self.limit + 1;
        Ok(buf.len())
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

struct MakeTruncatingStderr;

impl<'a> fmt::MakeWriter<'a> for MakeTruncatingStderr {
    type Writer = Box<dyn Write + 'a>;

    fn make_writer(&'a self) -> Self::Writer {
        match max_event_bytes() {
            Some(limit) => Box::new(TruncatingWriter::new(io::stderr(), limit)),
            None => Box::new(io::stderr()),
        }
    }
}

/// Default tracing filter directives applied when the user hasn't
/// configured their own via `RUST_LOG` or the `--log-level` CLI flag.
///
/// Rationale for each directive:
///
/// - `info`: baseline level for everything else.
/// - `matrix_sdk::latest_events=warn`: the latest-events subsystem logs a
///   `timer!` event at INFO for every `listen_to_room` call. With ~200+
///   rooms this floods the log on startup with one line per room even
///   though each call takes sub-microseconds.
const DEFAULT_LOG_DIRECTIVES: &str = "info,matrix_sdk::latest_events=warn";

/// Initialize the global tracing subscriber with stderr output.
///
/// `level` is a comma-separated filter string compatible with `EnvFilter`,
/// e.g. `"warn,ui=info,net=debug"`. If empty, `RUST_LOG` is honoured, or
/// `DEFAULT_LOG_DIRECTIVES` if that too is unset.
///
/// `to_stderr` enables colored stderr output.
///
/// `enable_debug` overrides all filters to `trace`.
pub fn init_logging(level: &str, to_stderr: bool, enable_debug: bool) {
    INIT.get_or_init(|| {
        let _ = LogTracer::init();

        let filter = if enable_debug {
            EnvFilter::builder().parse_lossy("trace")
        } else if !level.is_empty() {
            EnvFilter::builder().parse_lossy(level)
        } else {
            // No CLI flag: honour RUST_LOG if set, otherwise fall back to
            // our curated baseline. We deliberately don't merge the two —
            // if the user has set RUST_LOG they get full control and can
            // opt in to the suppressed targets again.
            let from_env = std::env::var("RUST_LOG").ok();
            let directives = from_env
                .as_deref()
                .map(str::trim)
                .filter(|value| !value.is_empty())
                .unwrap_or(DEFAULT_LOG_DIRECTIVES);
            EnvFilter::builder().parse_lossy(directives)
        };

        if to_stderr {
            let stderr_layer = fmt::layer()
                .with_ansi(true)
                .with_writer(MakeTruncatingStderr);

            let _ = tracing_subscriber::registry()
                .with(filter)
                .with(stderr_layer)
                .try_init();
        } else {
            let _ = tracing_subscriber::registry()
                .with(filter)
                .try_init();
        }
    });
}

/// Emit a tracing event from C++ using the component name as the tracing target
/// so that `EnvFilter` directives like `ui=off` or `db=debug` work correctly.
pub fn log_from_cpp(component: &str, level: &str, message: &str) {
    // tracing macros require `target:` to be a string literal, so we dispatch
    // on the known component names.  Unknown components fall back to "cpp".
    macro_rules! emit {
        ($lvl:ident, $msg:expr) => {
            match component {
                "ui"     => tracing::$lvl!(target: "ui",     "{}", $msg),
                "db"     => tracing::$lvl!(target: "db",     "{}", $msg),
                "net"    => tracing::$lvl!(target: "net",    "{}", $msg),
                "crypto" => tracing::$lvl!(target: "crypto", "{}", $msg),
                "qml"    => tracing::$lvl!(target: "qml",    "{}", $msg),
                "rust"   => tracing::$lvl!(target: "rust",   "{}", $msg),
                _        => tracing::$lvl!(target: "cpp",    "{}", $msg),
            }
        };
    }

    match level {
        "trace" => emit!(trace, message),
        "debug" => emit!(debug, message),
        "warn" => emit!(warn, message),
        "error" | "critical" => emit!(error, message),
        _ => emit!(info, message),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn written_through(limit: usize, chunks: &[&[u8]]) -> Vec<u8> {
        let mut sink = Vec::new();
        {
            let mut writer = TruncatingWriter::new(&mut sink, limit);
            for chunk in chunks {
                assert_eq!(writer.write(chunk).unwrap(), chunk.len());
            }
        }
        sink
    }

    #[test]
    fn short_event_passes_through_unchanged() {
        let out = written_through(64, &[b"hello world\n"]);
        assert_eq!(out, b"hello world\n");
    }

    #[test]
    fn event_at_exactly_the_limit_is_not_truncated() {
        let out = written_through(5, &[b"12345"]);
        assert_eq!(out, b"12345");
    }

    #[test]
    fn oversized_event_is_cut_with_a_marker() {
        let out = written_through(5, &[b"1234567890"]);
        let text = String::from_utf8(out).unwrap();
        assert!(text.starts_with("12345\u{1b}[0m ... [log event truncated"));
        assert!(!text.contains('6'));
    }

    #[test]
    fn writes_after_truncation_are_swallowed() {
        let out = written_through(5, &[b"1234567890", b"tail", b"\n"]);
        let text = String::from_utf8(out).unwrap();
        assert!(!text.contains("tail"));
        assert!(text.ends_with("KOMAI_LOG_MAX_EVENT_BYTES=0 to disable]\n"));
    }

    #[test]
    fn cut_lands_on_a_utf8_character_boundary() {
        // 'я' is two bytes; a limit of 5 would split the third one.
        let out = written_through(5, &["яяя".as_bytes()]);
        let text = String::from_utf8(out).unwrap();
        assert!(text.starts_with("яя\u{1b}[0m"));
    }

    #[test]
    fn multi_chunk_events_share_the_budget() {
        let out = written_through(8, &[b"12345", b"67890"]);
        let text = String::from_utf8(out).unwrap();
        assert!(text.starts_with("12345678\u{1b}[0m ... [log event truncated"));
    }
}
