// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logging/Logging.h"
#include "config/komai.h"
#include "komai-rust-cxxbridge/ffi.h"

#include <QString>
#include <QtGlobal>

namespace {
static std::shared_ptr<nhlog::Logger> db_logger     = nullptr;
static std::shared_ptr<nhlog::Logger> net_logger    = nullptr;
static std::shared_ptr<nhlog::Logger> crypto_logger = nullptr;
static std::shared_ptr<nhlog::Logger> ui_logger     = nullptr;
static std::shared_ptr<nhlog::Logger> qml_logger    = nullptr;
static std::shared_ptr<nhlog::Logger> rust_logger   = nullptr;

void
qmlMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    std::string localMsg = msg.toStdString();
    const char *file     = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";

    if (
      // The default style has the point size set. If you use pixel size anywhere, you get
      // that warning, which is useless, since sometimes you need the pixel size to match the
      // text to the size of the outer element for example. This is done in the avatar and
      // without that you get one warning for every Avatar displayed, which is stupid!
      msg.endsWith(QStringLiteral("Both point size and pixel size set. Using pixel size.")) ||
      // Qt SVG renderer strictness warnings from internal path parsing (Qt 6.11.0)
      msg.contains(QStringLiteral("Invalid path data; path truncated")) ||
      // Qt Particles module property shadowing warnings (Qt 6.11.0)
      msg.startsWith(QStringLiteral("Member enabled of the object QQuickParticleEmitter")) ||
      msg.startsWith(QStringLiteral("Member enabled of the object QQuickParticleAffector")) ||
      msg.startsWith(QStringLiteral("Member rotation of the object QQuickImageParticle")))
        return;

    switch (type) {
    case QtDebugMsg:
        qml_logger->debug("{} ({}:{}, {})", localMsg, file, context.line, function);
        break;
    case QtInfoMsg:
        qml_logger->info("{} ({}:{}, {})", localMsg, file, context.line, function);
        break;
    case QtWarningMsg:
        qml_logger->warn("{} ({}:{}, {})", localMsg, file, context.line, function);
        break;
    case QtCriticalMsg:
        qml_logger->critical("{} ({}:{}, {})", localMsg, file, context.line, function);
        break;
    case QtFatalMsg:
        qml_logger->critical("{} ({}:{}, {})", localMsg, file, context.line, function);
        break;
    }
}
}

namespace nhlog {

void
Logger::send(std::string_view level, std::string message)
{
    ::komai::rust::log_from_cpp(::rust::Str(component_.data(), component_.size()),
                                ::rust::Str(level.data(), level.size()),
                                ::rust::Str(message.data(), message.size()));
}

void
init(const QString &level, bool to_stderr)
{
    auto levelStd = level.toStdString();

    ::komai::rust::init_logging(
      ::rust::Str(levelStd.data(), levelStd.size()), to_stderr, komai::enable_debug_log);

    net_logger    = std::make_shared<Logger>("net");
    ui_logger     = std::make_shared<Logger>("ui");
    db_logger     = std::make_shared<Logger>("db");
    crypto_logger = std::make_shared<Logger>("crypto");
    qml_logger    = std::make_shared<Logger>("qml");
    rust_logger   = std::make_shared<Logger>("rust");

    qInstallMessageHandler(qmlMessageHandler);
}

std::shared_ptr<Logger>
ui()
{
    return ui_logger;
}

std::shared_ptr<Logger>
net()
{
    return net_logger;
}

std::shared_ptr<Logger>
db()
{
    return db_logger;
}

std::shared_ptr<Logger>
crypto()
{
    return crypto_logger;
}

std::shared_ptr<Logger>
qml()
{
    return qml_logger;
}

std::shared_ptr<Logger>
rust()
{
    return rust_logger;
}
}
