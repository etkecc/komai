// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace db {

enum class CursorOp
{
    First,
    FirstDup,
    GetBoth,
    Last,
    Next,
    NextDup,
    NextNoDup,
    Prev,
    Set,
    SetRange,
};

} // namespace db
