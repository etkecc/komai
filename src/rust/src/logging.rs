// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    fmt,
    sync::OnceLock,
};

use tracing::{Event, Subscriber, field::Field};
use tracing_log::LogTracer;
use tracing_subscriber::{
    layer::{Context, Layer},
    prelude::*,
};

use crate::ffi;

pub fn ensure_initialized() {
    static INIT: OnceLock<()> = OnceLock::new();
    INIT.get_or_init(|| {
        let _ = LogTracer::init();
        let subscriber = tracing_subscriber::registry().with(CxxBridgeLayer);
        let _ = tracing::subscriber::set_global_default(subscriber);
    });
}

struct CxxBridgeLayer;

impl<S> Layer<S> for CxxBridgeLayer
where
    S: Subscriber,
{
    fn on_event(&self, event: &Event<'_>, _ctx: Context<'_, S>) {
        let metadata = event.metadata();
        let mut visitor = EventFieldVisitor::default();
        event.record(&mut visitor);

        let message = visitor.finish();
        if message.is_empty() {
            return;
        }

        let level = match *metadata.level() {
            tracing::Level::TRACE => "trace",
            tracing::Level::DEBUG => "debug",
            tracing::Level::INFO => "info",
            tracing::Level::WARN => "warn",
            tracing::Level::ERROR => "error",
        };

        ffi::matrix_log_event(
            level,
            metadata.target(),
            metadata.module_path().unwrap_or_default(),
            metadata.file().unwrap_or_default(),
            metadata.line().unwrap_or_default(),
            &message,
        );
    }
}

#[derive(Default)]
struct EventFieldVisitor {
    message: Option<String>,
    fields: Vec<String>,
}

impl EventFieldVisitor {
    fn record_value(&mut self, field: &Field, value: String) {
        if field.name() == "message" {
            if !value.is_empty() {
                self.message = Some(value);
            }
            return;
        }

        self.fields.push(format!("{}={value}", field.name()));
    }

    fn finish(self) -> String {
        match (self.message, self.fields.is_empty()) {
            (Some(message), true) => message,
            (Some(message), false) => format!("{message} {}", self.fields.join(" ")),
            (None, false) => self.fields.join(" "),
            (None, true) => String::new(),
        }
    }
}

impl tracing::field::Visit for EventFieldVisitor {
    fn record_str(&mut self, field: &Field, value: &str) {
        self.record_value(field, value.to_owned());
    }

    fn record_debug(&mut self, field: &Field, value: &dyn fmt::Debug) {
        self.record_value(field, format!("{value:?}"));
    }
}
