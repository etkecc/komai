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

inline constexpr std::array<const char *, 5> CUSTOM_KEYS = {
  "attention",
  "attentionText",
  "success",
  "warning",
  "error",
};

using Palette = std::map<std::string, std::string>;

// Parse a hex color string (with or without #) to Rgb
Rgb
parseColor(const std::string &hex);

// Convert Rgb to #-prefixed 7-char lowercase hex (e.g. "#ab12ef")
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

// Darken/lighten a foreground until it reaches target contrast across all backgrounds.
std::string
adjustFgForBackgrounds(const std::string &fgHex,
                       const std::vector<std::string> &backgrounds,
                       const std::string &variant,
                       double target);

// Fix poor contrast in automatic Base16 mapping
void
ensureContrast(Palette &mapping, const Palette &palette, const std::string &variant);

// Map Base16 slots to QPalette role colors (16 keys)
Palette
base16ToPalette(const Palette &palette, const std::string &variant);

// Map Base16 slots to semantic accent colors (4 keys)
Palette
base16ToCustom(const Palette &palette);

// Pick a readable text color to pair with `attention`. Prefers `preferred` when
// it already gives ≥4.5 contrast (so themes can keep a tinted dark/light tone
// instead of pure black/white); otherwise picks whichever of black/white
// contrasts better against `attention`.
std::string
deriveAttentionText(const std::string &attention, const std::string &preferred);

// Guess light/dark variant from base00 luminance
std::string
detectVariant(const Palette &palette);

// Strip trailing " dark"/" light" (case-insensitive) from a theme name
std::string
stripVariantSuffix(const std::string &name);

// Theme-defined colors for a bubble slot. `background` is required; the rest
// are optional overrides that fall back to the global palette roles.
struct UserColorSlot
{
    std::string background;
    std::string text;
    std::string secondaryText;
    std::string link;
};

// Result of user color generation for themes
struct UserColors
{
    UserColorSlot self;                // colors for "our own" bubble slot
    std::vector<UserColorSlot> others; // colors for other members (per-member palette)
};

// Generate literal bubble-fill userColors from a highlight (accent), base surface,
// and variant. The returned values are already softened for direct bubble use.
UserColors
generateUserColors(const std::string &highlightHex,
                   const std::string &baseHex,
                   const std::string &variant);

// Populate explicit per-bubble foreground overrides using the theme palette as
// the default source and adjusting each role against each bubble background.
void
populateUserColorForegrounds(UserColors &userColors,
                             const Palette &palette,
                             const std::string &variant);

} // namespace theme_color
