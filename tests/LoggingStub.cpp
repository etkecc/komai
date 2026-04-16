// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include <QString>

#include "logging/Logging.h"

// Stub implementation for test targets that do not link the Rust library.
// Logger::send() is a no-op so log calls silently discard their output.

namespace komai::logging {

void
Logger::send(std::string_view /*level*/, std::string /*message*/)
{}

static auto stub_ui     = std::make_shared<Logger>("ui");
static auto stub_net    = std::make_shared<Logger>("net");
static auto stub_db     = std::make_shared<Logger>("db");
static auto stub_crypto = std::make_shared<Logger>("crypto");
static auto stub_qml    = std::make_shared<Logger>("qml");
static auto stub_rust   = std::make_shared<Logger>("rust");

std::shared_ptr<Logger>
ui()
{
    return stub_ui;
}

std::shared_ptr<Logger>
net()
{
    return stub_net;
}

std::shared_ptr<Logger>
db()
{
    return stub_db;
}

std::shared_ptr<Logger>
crypto()
{
    return stub_crypto;
}

std::shared_ptr<Logger>
qml()
{
    return stub_qml;
}

std::shared_ptr<Logger>
rust()
{
    return stub_rust;
}

void
init(const QString &, bool)
{}

} // namespace komai::logging
