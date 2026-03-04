// SPDX-FileCopyrightText: Komai Contributors
// SPDX-FileCopyrightText: Boring Avatars Contributors (MIT License)
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Avatar generation algorithms ported from Boring Avatars by boringdesigners.
// https://github.com/boringdesigners/boring-avatars — MIT License.
// Color palette expanded beyond the original 5-color default for greater variety.

#pragma once

#include <QString>

#include <array>
#include <cmath>
#include <cstdint>

namespace boring_avatars {

constexpr std::array<const char *, 10> kDefaultColors = {
  "#0A0310",
  "#49007E",
  "#FF005B",
  "#FF7D10",
  "#FFB238",
  "#1B4965",
  "#2A9D8F",
  "#5B8E7D",
  "#3A86C8",
  "#8338EC",
};

inline uint32_t
hashCode(const QString &name)
{
    int32_t hash = 0;
    for (int i = 0; i < name.length(); ++i) {
        const auto character = static_cast<int32_t>(name.at(i).unicode());
        hash                 = ((hash << 5) - hash) + character;
        hash                 = hash & hash; // 32-bit truncation
    }
    return static_cast<uint32_t>(std::abs(hash));
}

inline int
getDigit(uint32_t number, int ntn)
{
    return static_cast<int>(std::fmod(std::floor(number / std::pow(10.0, ntn)), 10.0));
}

inline bool
getBoolean(uint32_t number, int ntn)
{
    return (getDigit(number, ntn) % 2) == 0;
}

inline int
getUnit(uint32_t number, int range, int index = 0)
{
    const int value = static_cast<int>(number % static_cast<uint32_t>(range));
    if (index != 0 && (getDigit(number, index) % 2) == 0)
        return -value;
    return value;
}

inline const char *
getRandomColor(uint32_t number, int range)
{
    return kDefaultColors[number % static_cast<uint32_t>(range)];
}

inline const char *
getContrast(const char *hexcolor)
{
    // Skip leading '#' if present
    const char *hex = hexcolor;
    if (hex[0] == '#')
        hex++;

    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return 0;
    };

    const int r = hexVal(hex[0]) * 16 + hexVal(hex[1]);
    const int g = hexVal(hex[2]) * 16 + hexVal(hex[3]);
    const int b = hexVal(hex[4]) * 16 + hexVal(hex[5]);

    const int yiq = (r * 299 + g * 587 + b * 114) / 1000;
    return (yiq >= 128) ? "#000000" : "#FFFFFF";
}

// Generator functions — each returns an SVG string for the given key.
// SIZE is the SVG viewBox dimension (caller renders at requested pixel size).
QString
generateBeam(const QString &key);
QString
generateMarble(const QString &key);
QString
generateBauhaus(const QString &key);

} // namespace boring_avatars
