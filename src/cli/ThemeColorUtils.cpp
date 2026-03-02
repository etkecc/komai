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
      {"red", getOr(palette, "base08", "a82353")},
      {"green", getOr(palette, "base0B", "008000")},
      {"orange", getOr(palette, "base09", "fcbe05")},
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

} // namespace theme_color
