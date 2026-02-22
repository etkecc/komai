// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <QString>

namespace nhlog {

std::shared_ptr<spdlog::logger>
ui()
{
    return spdlog::default_logger();
}

std::shared_ptr<spdlog::logger>
net()
{
    return spdlog::default_logger();
}

std::shared_ptr<spdlog::logger>
db()
{
    return spdlog::default_logger();
}

std::shared_ptr<spdlog::logger>
crypto()
{
    return spdlog::default_logger();
}

std::shared_ptr<spdlog::logger>
qml()
{
    return spdlog::default_logger();
}

void
init(const QString &, const QString &, bool)
{}

} // namespace nhlog
