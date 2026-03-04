// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Direct port of bin/theme/colors.py — color math and Base16→QPalette mapping.

#include "ThemeColorUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace theme_color {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string
getOr(const Palette &p, const std::string &key, const std::string &fallback)
{
    auto it = p.find(key);
    return (it != p.end()) ? it->second : fallback;
}

// ---------------------------------------------------------------------------
// Color utilities
// ---------------------------------------------------------------------------

Rgb
parseColor(const std::string &hex)
{
    std::string h = hex;
    if (!h.empty() && h[0] == '#')
        h = h.substr(1);
    if (h.size() != 6)
        throw std::invalid_argument("Invalid hex color: " + hex);
    auto fromHex = [](const std::string &s, int pos) {
        return static_cast<int>(std::stoul(s.substr(pos, 2), nullptr, 16));
    };
    return {fromHex(h, 0), fromHex(h, 2), fromHex(h, 4)};
}

std::string
rgbToHex(const Rgb &c)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02x%02x%02x", c.r & 0xff, c.g & 0xff, c.b & 0xff);
    return buf;
}

double
linearize(int v)
{
    double s = v / 255.0;
    return (s <= 0.04045) ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

int
delinearize(double v)
{
    v        = std::clamp(v, 0.0, 1.0);
    double s = (v <= 0.0031308) ? v * 12.92 : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
    return std::clamp(static_cast<int>(std::round(s * 255)), 0, 255);
}

double
luminance(const std::string &hex)
{
    auto [r, g, b] = parseColor(hex);
    return 0.2126 * linearize(r) + 0.7152 * linearize(g) + 0.0722 * linearize(b);
}

double
contrastRatio(const std::string &hex1, const std::string &hex2)
{
    double l1      = luminance(hex1);
    double l2      = luminance(hex2);
    double lighter = std::max(l1, l2);
    double darker  = std::min(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
}

std::string
blendToward(const std::string &hex, const std::string &target, double t)
{
    auto [r1, g1, b1] = parseColor(hex);
    auto [r2, g2, b2] = parseColor(target);
    double lr         = linearize(r1) + (linearize(r2) - linearize(r1)) * t;
    double lg         = linearize(g1) + (linearize(g2) - linearize(g1)) * t;
    double lb         = linearize(b1) + (linearize(b2) - linearize(b1)) * t;
    return rgbToHex({delinearize(lr), delinearize(lg), delinearize(lb)});
}

FgCandidate
bestFgCandidate(const std::string &bgHex, const std::vector<std::string> &candidates)
{
    FgCandidate best{"", 0.0};
    for (const auto &c : candidates) {
        if (c.empty())
            continue;
        double r = contrastRatio(c, bgHex);
        if (r > best.ratio) {
            best.hex   = c;
            best.ratio = r;
        }
    }
    return best;
}

std::string
adjustBgForContrast(const std::string &bgHex, const std::string &fgHex, double target)
{
    if (contrastRatio(fgHex, bgHex) >= target)
        return bgHex;

    std::string toward = (luminance(fgHex) > luminance(bgHex)) ? "000000" : "ffffff";

    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < 30; ++i) {
        double mid    = (lo + hi) / 2;
        auto adjusted = blendToward(bgHex, toward, mid);
        if (contrastRatio(fgHex, adjusted) >= target)
            hi = mid;
        else
            lo = mid;
    }

    return blendToward(bgHex, toward, hi);
}

// ---------------------------------------------------------------------------
// Contrast fixing (port of _ensure_contrast)
// ---------------------------------------------------------------------------

void
ensureContrast(Palette &mapping, const Palette &palette, const std::string &variant)
{
    constexpr double MIN_TEXT_ON_ACCENT = 3.0;
    constexpr double MIN_HOVER_DISTINCT = 1.5;

    std::vector<std::string> textCandidates;
    if (variant == "dark") {
        textCandidates = {
          getOr(palette, "base07", ""),
          getOr(palette, "base06", ""),
          getOr(palette, "base05", ""),
          "ffffff",
        };
    } else {
        textCandidates = {
          getOr(palette, "base00", ""),
          getOr(palette, "base01", ""),
          getOr(palette, "base02", ""),
          "000000",
        };
    }

    // highlightedText on highlight (selected items)
    if (contrastRatio(mapping["highlightedText"], mapping["highlight"]) < MIN_TEXT_ON_ACCENT) {
        auto [bestHt, bestRatio] = bestFgCandidate(mapping["highlight"], textCandidates);
        if (!bestHt.empty())
            mapping["highlightedText"] = bestHt;
        if (bestRatio < MIN_TEXT_ON_ACCENT) {
            mapping["highlight"] = adjustBgForContrast(
              mapping["highlight"], mapping["highlightedText"], MIN_TEXT_ON_ACCENT);
            mapping["link"] = mapping["highlight"];
        }
    }

    // brightText on dark (hover states)
    if (contrastRatio(mapping["brightText"], mapping["dark"]) < MIN_TEXT_ON_ACCENT) {
        auto [bestBt, _] = bestFgCandidate(mapping["dark"], textCandidates);
        if (!bestBt.empty())
            mapping["brightText"] = bestBt;
    }

    // Hover background (dark) must be visually distinct from both window and
    // button, and buttonText should remain readable on it.
    constexpr double MIN_TEXT_ON_HOVER  = 2.5;
    constexpr double MAX_HOVER_CONTRAST = 3.0;
    std::string toward                  = (variant == "light") ? "000000" : "ffffff";

    auto hoverDistinct = [&](const std::string &candidate) {
        return contrastRatio(candidate, mapping["window"]) >= MIN_HOVER_DISTINCT &&
               contrastRatio(candidate, mapping["button"]) >= MIN_HOVER_DISTINCT;
    };

    if (!hoverDistinct(mapping["dark"])) {
        bool found = false;
        for (const auto &slot : {"base02", "base03"}) {
            auto it = palette.find(slot);
            if (it != palette.end() && hoverDistinct(it->second)) {
                mapping["dark"] = it->second;
                found           = true;
                break;
            }
        }
        if (!found) {
            // Find minimal blend from button for hover distinction
            double lo = 0.0, hi = 1.0;
            for (int i = 0; i < 30; ++i) {
                double mid     = (lo + hi) / 2;
                auto candidate = blendToward(mapping["button"], toward, mid);
                if (hoverDistinct(candidate))
                    hi = mid;
                else
                    lo = mid;
            }
            mapping["dark"] = blendToward(mapping["button"], toward, hi);
        }
    }

    // If buttonText is hard to read on hover, push dark further but cap
    if (contrastRatio(mapping["buttonText"], mapping["dark"]) < MIN_TEXT_ON_HOVER) {
        // Find the allowed range
        double lo = 0.0, hi = 1.0;
        for (int i = 0; i < 30; ++i) {
            double mid     = (lo + hi) / 2;
            auto candidate = blendToward(mapping["button"], toward, mid);
            if (contrastRatio(candidate, mapping["window"]) <= MAX_HOVER_CONTRAST)
                lo = mid;
            else
                hi = mid;
        }
        double maxBlend = lo;

        // Scan the allowed range for best buttonText readability
        auto bestDark = mapping["dark"];
        double bestCr = contrastRatio(mapping["buttonText"], mapping["dark"]);
        for (int i = 0; i <= 50; ++i) {
            double t       = maxBlend * i / 50.0;
            auto candidate = blendToward(mapping["button"], toward, t);
            if (!hoverDistinct(candidate))
                continue;
            double cr = contrastRatio(mapping["buttonText"], candidate);
            if (cr > bestCr) {
                bestCr   = cr;
                bestDark = candidate;
            }
        }
        mapping["dark"] = bestDark;
    }
}

// ---------------------------------------------------------------------------
// Base16 → QPalette mapping
// ---------------------------------------------------------------------------

Palette
base16ToPalette(const Palette &palette, const std::string &variant)
{
    auto get = [&](const std::string &key) -> std::string {
        auto it = palette.find(key);
        return (it != palette.end()) ? it->second : std::string{};
    };

    Palette mapping;
    mapping["window"]        = get("base00");
    mapping["windowText"]    = get("base05");
    mapping["base"]          = getOr(palette, "base01", get("base00"));
    mapping["alternateBase"] = getOr(palette, "base02", getOr(palette, "base01", get("base00")));
    mapping["text"]          = get("base05");
    mapping["brightText"]    = getOr(palette, "base07", getOr(palette, "base06", get("base05")));
    mapping["button"]        = getOr(palette, "base01", get("base00"));
    mapping["buttonText"]    = getOr(palette, "base04", getOr(palette, "base03", get("base05")));
    mapping["light"]         = getOr(palette, "base06", getOr(palette, "base05", "ffffff"));
    mapping["mid"]           = getOr(palette, "base03", getOr(palette, "base02", get("base01")));
    mapping["dark"]          = getOr(palette, "base01", get("base00"));
    mapping["highlight"]     = getOr(palette, "base0D", "38a3d8");
    mapping["highlightedText"] =
      (variant == "dark") ? getOr(palette, "base07", "ffffff") : getOr(palette, "base00", "ffffff");
    mapping["link"]        = getOr(palette, "base0D", "38a3d8");
    mapping["toolTipBase"] = getOr(palette, "base01", get("base00"));
    mapping["toolTipText"] = get("base05");

    ensureContrast(mapping, palette, variant);

    return mapping;
}

Palette
base16ToCustom(const Palette &palette)
{
    return {
      {"attention", getOr(palette, "base08", "a82353")},
      {"success", getOr(palette, "base0B", "008000")},
      {"warning", getOr(palette, "base09", "fcbe05")},
      {"error", getOr(palette, "base08", "dd3d3d")},
    };
}

std::string
detectVariant(const Palette &palette)
{
    auto it        = palette.find("base00");
    std::string h  = (it != palette.end()) ? it->second : "000000";
    auto [r, g, b] = parseColor(h);
    double luma    = 0.299 * r + 0.587 * g + 0.114 * b;
    return (luma > 128) ? "light" : "dark";
}

std::string
stripVariantSuffix(const std::string &name)
{
    auto lower = name;
    std::transform(
      lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    for (const auto &suffix : {" dark", " light"}) {
        auto slen = std::strlen(suffix);
        if (lower.size() >= slen && lower.substr(lower.size() - slen) == suffix)
            return name.substr(0, name.size() - slen);
    }
    return name;
}

// ---------------------------------------------------------------------------
// User color generation
// ---------------------------------------------------------------------------

// 16 maximally-spaced hues (golden-angle inspired) — same ordering as the
// former runtime kPaletteHues table, so existing visual habits are preserved.
static constexpr std::array<double, 16> kGoldenAngleHues = {
  0,
  137.5,
  275,
  52.5,
  190,
  327.5,
  95,
  232.5,
  22.5,
  160,
  297.5,
  75,
  212.5,
  350,
  117.5,
  255,
};

// Convert HSL (h in 0-360, s/l in 0-1) to RGB hex string.
static std::string
hslToHex(double h, double s, double l)
{
    // Normalize hue to [0, 360)
    h = std::fmod(h, 360.0);
    if (h < 0)
        h += 360.0;

    auto hueToRgb = [](double p, double q, double t) -> double {
        if (t < 0)
            t += 1;
        if (t > 1)
            t -= 1;
        if (t < 1.0 / 6)
            return p + (q - p) * 6 * t;
        if (t < 1.0 / 2)
            return q;
        if (t < 2.0 / 3)
            return p + (q - p) * (2.0 / 3 - t) * 6;
        return p;
    };

    double q     = (l < 0.5) ? l * (1 + s) : l + s - l * s;
    double p     = 2 * l - q;
    double hNorm = h / 360.0;

    int r = std::clamp(static_cast<int>(std::round(hueToRgb(p, q, hNorm + 1.0 / 3) * 255)), 0, 255);
    int g = std::clamp(static_cast<int>(std::round(hueToRgb(p, q, hNorm) * 255)), 0, 255);
    int b = std::clamp(static_cast<int>(std::round(hueToRgb(p, q, hNorm - 1.0 / 3) * 255)), 0, 255);

    return rgbToHex({r, g, b});
}

// Extract hue (0-360) from a hex color using HSL conversion.
static double
hueFromHex(const std::string &hex)
{
    auto [r, g, b] = parseColor(hex);
    double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
    double maxC  = std::max({rf, gf, bf});
    double minC  = std::min({rf, gf, bf});
    double delta = maxC - minC;

    if (delta < 1e-6)
        return 0.0; // achromatic

    double h = 0;
    if (maxC == rf)
        h = 60.0 * std::fmod((gf - bf) / delta, 6.0);
    else if (maxC == gf)
        h = 60.0 * ((bf - rf) / delta + 2.0);
    else
        h = 60.0 * ((rf - gf) / delta + 4.0);

    if (h < 0)
        h += 360.0;
    return h;
}

UserColors
generateUserColors(const std::string &highlightHex, const std::string &variant)
{
    UserColors result;
    result.self = highlightHex;

    double selfHue                  = hueFromHex(highlightHex);
    constexpr double kExclusionZone = 30.0; // degrees on each side of self hue

    // Variant-aware saturation and lightness
    double sat, lit;
    if (variant == "dark") {
        sat = 0.65;
        lit = 0.60;
    } else {
        sat = 0.70;
        lit = 0.40;
    }

    // Filter hues that are too close to the self color
    std::vector<double> filteredHues;
    filteredHues.reserve(kGoldenAngleHues.size());
    for (double h : kGoldenAngleHues) {
        double diff = std::abs(h - selfHue);
        if (diff > 180.0)
            diff = 360.0 - diff;
        if (diff >= kExclusionZone)
            filteredHues.push_back(h);
    }

    // Fallback: if too many hues were filtered, use the full set
    if (filteredHues.size() < 8) {
        filteredHues.clear();
        for (double h : kGoldenAngleHues)
            filteredHues.push_back(h);
    }

    result.others.reserve(filteredHues.size());
    for (double h : filteredHues)
        result.others.push_back(hslToHex(h, sat, lit));

    return result;
}

} // namespace theme_color
