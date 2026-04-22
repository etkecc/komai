// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::OnceLock;

use tracing_log::LogTracer;
use tracing_subscriber::{
    fmt,
    layer::SubscriberExt,
    util::SubscriberInitExt,
    EnvFilter,
};

static INIT: OnceLock<()> = OnceLock::new();

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
                .with_writer(std::io::stderr);

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
