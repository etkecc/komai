// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// DO NOT EDIT Provider.h DIRECTLY! EDIT IT IN bin/emoji/codegen.py AND RUN just emoji-generate!

#pragma once
#include "Emoji.h"
#include <array>

namespace emoji {
class Provider
{
public:
    // all emoji for QML purposes
    static const std::array<Emoji, 3799> emoji;
};
} // namespace emoji
