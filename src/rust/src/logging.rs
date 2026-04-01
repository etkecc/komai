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

/// Initialize the global tracing subscriber with file and/or stderr sinks.
///
/// `level` is a comma-separated filter string compatible with `EnvFilter`,
/// e.g. `"warn,ui=info,net=debug"`.
///
/// `log_file_path` is the path for the log file. Pass an empty string
/// to disable file logging.
///
/// `to_stderr` enables colored stderr output.
///
/// `enable_debug` overrides all filters to `trace`.
pub fn init_logging(level: &str, log_file_path: &str, to_stderr: bool, enable_debug: bool) {
    INIT.get_or_init(|| {
        let _ = LogTracer::init();

        let filter_str = if enable_debug {
            "trace".to_owned()
        } else if level.is_empty() {
            "info".to_owned()
        } else {
            level.to_owned()
        };

        let filter = EnvFilter::builder()
            .parse_lossy(&filter_str);

        if !log_file_path.is_empty() && to_stderr {
            let file_appender = make_file_appender(log_file_path);
            let (non_blocking, guard) = tracing_appender::non_blocking(file_appender);
            std::mem::forget(guard);

            let file_layer = fmt::layer()
                .with_ansi(false)
                .with_writer(non_blocking);

            let stderr_layer = fmt::layer()
                .with_ansi(true)
                .with_writer(std::io::stderr);

            let _ = tracing_subscriber::registry()
                .with(filter)
                .with(file_layer)
                .with(stderr_layer)
                .try_init();
        } else if !log_file_path.is_empty() {
            let file_appender = make_file_appender(log_file_path);
            let (non_blocking, guard) = tracing_appender::non_blocking(file_appender);
            std::mem::forget(guard);

            let file_layer = fmt::layer()
                .with_ansi(false)
                .with_writer(non_blocking);

            let _ = tracing_subscriber::registry()
                .with(filter)
                .with(file_layer)
                .try_init();
        } else if to_stderr {
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

/// Emit a tracing event from C++ at the given level with the given component as
/// the target.
pub fn log_from_cpp(component: &str, level: &str, message: &str) {
    match level {
        "trace" => tracing::trace!(target: "cpp", component = component, "{}", message),
        "debug" => tracing::debug!(target: "cpp", component = component, "{}", message),
        "warn" => tracing::warn!(target: "cpp", component = component, "{}", message),
        "error" | "critical" => {
            tracing::error!(target: "cpp", component = component, "{}", message)
        }
        _ => tracing::info!(target: "cpp", component = component, "{}", message),
    }
}

fn make_file_appender(log_file_path: &str) -> tracing_appender::rolling::RollingFileAppender {
    use std::path::Path;

    let path = Path::new(log_file_path);
    let dir = path.parent().unwrap_or(Path::new("."));
    let filename = path
        .file_name()
        .map(|f| f.to_string_lossy().into_owned())
        .unwrap_or_else(|| "komai.log".to_owned());

    tracing_appender::rolling::Builder::new()
        .rotation(tracing_appender::rolling::Rotation::NEVER)
        .filename_prefix(filename)
        .filename_suffix("")
        .build(dir)
        .expect("failed to create rolling file appender")
}
