// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <format>
#include <memory>
#include <string>
#include <string_view>

#include <QString>

namespace nhlog {

/// Lightweight logger proxy that forwards formatted messages to the Rust tracing
/// subscriber via CXX FFI while preserving the existing `nhlog::ui()->info(...)`
/// call shape.
class Logger
{
public:
    explicit Logger(std::string component)
      : component_(std::move(component))
    {
    }

    template<typename... Args>
    void trace(std::format_string<Args...> fmt, Args &&...args)
    {
        send("trace", std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args &&...args)
    {
        send("debug", std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args &&...args)
    {
        send("info", std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warn(std::format_string<Args...> fmt, Args &&...args)
    {
        send("warn", std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args &&...args)
    {
        send("error", std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void critical(std::format_string<Args...> fmt, Args &&...args)
    {
        send("critical", std::format(fmt, std::forward<Args>(args)...));
    }

    // Single-string overloads for call sites that pass a runtime std::string
    // directly (e.g. nhlog::ui()->error(someString)).
    void trace(std::string_view msg) { send("trace", std::string(msg)); }
    void debug(std::string_view msg) { send("debug", std::string(msg)); }
    void info(std::string_view msg) { send("info", std::string(msg)); }
    void warn(std::string_view msg) { send("warn", std::string(msg)); }
    void error(std::string_view msg) { send("error", std::string(msg)); }
    void critical(std::string_view msg) { send("critical", std::string(msg)); }

    /// No-op: Rust tracing-appender handles flushing internally.
    void flush() {}

private:
    void send(std::string_view level, std::string message);

    std::string component_;
};

void
init(const QString &level, const QString &path, bool to_stderr);

std::shared_ptr<Logger>
ui();

std::shared_ptr<Logger>
net();

std::shared_ptr<Logger>
db();

std::shared_ptr<Logger>
crypto();

std::shared_ptr<Logger>
qml();

std::shared_ptr<Logger>
rust();

}
