// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

namespace theme_color {

struct Rgb
{
    int r, g, b;
};

// Canonical palette key lists (match bin/theme/colors.py)
inline constexpr std::array<const char *, 16> PALETTE_KEYS = {
  "window",
  "windowText",
  "base",
  "alternateBase",
  "text",
  "brightText",
  "button",
  "buttonText",
  "light",
  "mid",
  "dark",
  "highlight",
  "highlightedText",
  "link",
  "toolTipBase",
  "toolTipText",
};

inline constexpr std::array<const char *, 4> CUSTOM_KEYS = {
  "red",
  "green",
  "orange",
  "error",
};

using Palette = std::map<std::string, std::string>;

// Parse a hex color string (6 chars, no #) to Rgb
Rgb
parseColor(const std::string &hex);

// Convert Rgb back to 6-char lowercase hex
std::string
rgbToHex(const Rgb &c);

// sRGB linearization (WCAG 2.0)
double
linearize(int v);

// sRGB delinearization
int
delinearize(double v);

// Relative luminance per WCAG 2.0
double
luminance(const std::string &hex);

// WCAG 2.0 contrast ratio (always >= 1.0)
double
contrastRatio(const std::string &hex1, const std::string &hex2);

// Blend hex toward target by factor t (0.0=unchanged, 1.0=target) in linear space
std::string
blendToward(const std::string &hex, const std::string &target, double t);

// Pick the candidate with highest contrast against bg
struct FgCandidate
{
    std::string hex;
    double ratio;
};
FgCandidate
bestFgCandidate(const std::string &bgHex, const std::vector<std::string> &candidates);

// Darken/lighten bg until fg achieves target contrast
std::string
adjustBgForContrast(const std::string &bgHex, const std::string &fgHex, double target);

// Fix poor contrast in automatic Base16 mapping
void
ensureContrast(Palette &mapping, const Palette &palette, const std::string &variant);

// Map Base16 slots to QPalette role colors (16 keys)
Palette
base16ToPalette(const Palette &palette, const std::string &variant);

// Map Base16 slots to semantic accent colors (4 keys)
Palette
base16ToCustom(const Palette &palette);

// Guess light/dark variant from base00 luminance
std::string
detectVariant(const Palette &palette);

// Strip trailing " dark"/" light" (case-insensitive) from a theme name
std::string
stripVariantSuffix(const std::string &name);

} // namespace theme_color
