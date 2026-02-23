// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdexcept>
#include <string>

namespace db {

enum class ErrorKind
{
    Unknown,
    VersionMismatch,
    Invalid,
    MapFull,
    DbsFull,
};

class Error : public std::runtime_error
{
public:
    Error(std::string message, ErrorKind kind = ErrorKind::Unknown)
      : std::runtime_error(std::move(message))
      , kind_(kind)
    {
    }

    ErrorKind kind() const noexcept { return kind_; }

private:
    ErrorKind kind_ = ErrorKind::Unknown;
};

} // namespace db
