// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "cli/ThemeColorUtils.h"

static int failures = 0;

static bool
expect(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    ++failures;
    return false;
}

static void
testParseColor()
{
    auto c = theme_color::parseColor("1e1e2e");
    expect(c.r == 0x1e, "parseColor r");
    expect(c.g == 0x1e, "parseColor g");
    expect(c.b == 0x2e, "parseColor b");

    auto c2 = theme_color::parseColor("#ff8800");
    expect(c2.r == 0xff, "parseColor with # r");
    expect(c2.g == 0x88, "parseColor with # g");
    expect(c2.b == 0x00, "parseColor with # b");

    bool threw = false;
    try {
        theme_color::parseColor("abc");
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    expect(threw, "parseColor invalid throws");
}

static void
testRgbToHex()
{
    auto hex = theme_color::rgbToHex({0x1e, 0x1e, 0x2e});
    expect(hex == "#1e1e2e", "rgbToHex basic");

    auto hex2 = theme_color::rgbToHex({0xff, 0x00, 0xab});
    expect(hex2 == "#ff00ab", "rgbToHex leading zeros");
}

static void
testLuminance()
{
    expect(std::abs(theme_color::luminance("000000") - 0.0) < 0.001, "luminance black");
    expect(std::abs(theme_color::luminance("ffffff") - 1.0) < 0.001, "luminance white");
}

static void
testContrastRatio()
{
    auto cr = theme_color::contrastRatio("000000", "ffffff");
    expect(std::abs(cr - 21.0) < 0.1, "contrastRatio black/white ~21");

    auto cr1 = theme_color::contrastRatio("1e1e2e", "cdd6f4");
    auto cr2 = theme_color::contrastRatio("cdd6f4", "1e1e2e");
    expect(std::abs(cr1 - cr2) < 0.001, "contrastRatio symmetric");
}

static void
testBlendToward()
{
    auto noChange = theme_color::blendToward("ff8800", "000000", 0.0);
    expect(noChange == "#ff8800", "blendToward t=0 unchanged");

    auto fullChange = theme_color::blendToward("ff8800", "000000", 1.0);
    expect(fullChange == "#000000", "blendToward t=1 is target");

    // Blend white toward black at 0.5 in linear space
    auto mid = theme_color::blendToward("ffffff", "000000", 0.5);
    auto c   = theme_color::parseColor(mid);
    // In linear space, midpoint is 0.5, which delinearizes to ~188
    expect(c.r > 170 && c.r < 200, "blendToward midpoint in expected range");
    expect(c.r == c.g && c.r == c.b, "blendToward midpoint gray");
}

static void
testDetectVariant()
{
    theme_color::Palette dark = {{"base00", "#1e1e2e"}};
    expect(theme_color::detectVariant(dark) == "dark", "detectVariant dark");

    theme_color::Palette light = {{"base00", "#f5f5dc"}};
    expect(theme_color::detectVariant(light) == "light", "detectVariant light");
}

static void
testStripVariantSuffix()
{
    expect(theme_color::stripVariantSuffix("Catppuccin Mocha Dark") == "Catppuccin Mocha",
           "stripVariantSuffix dark");
    expect(theme_color::stripVariantSuffix("Solarized Light") == "Solarized",
           "stripVariantSuffix light");
    expect(theme_color::stripVariantSuffix("Nord") == "Nord", "stripVariantSuffix none");
    expect(theme_color::stripVariantSuffix("My Theme DARK") == "My Theme",
           "stripVariantSuffix case-insensitive");
}

static void
testBase16ToPalette()
{
    // catppuccin-mocha source_base16 as input
    theme_color::Palette base16 = {
      {"base00", "#1e1e2e"}, {"base01", "#181825"}, {"base02", "#313244"},
      {"base03", "#45475a"}, {"base04", "#585b70"}, {"base05", "#cdd6f4"},
      {"base06", "#f5e0dc"}, {"base07", "#b4befe"}, {"base08", "#f38ba8"},
      {"base09", "#fab387"}, {"base0A", "#f9e2af"}, {"base0B", "#a6e3a1"},
      {"base0C", "#94e2d5"}, {"base0D", "#89b4fa"}, {"base0E", "#cba6f7"},
      {"base0F", "#f2cdcd"},
    };

    auto result = theme_color::base16ToPalette(base16, "dark");

    expect(result["window"] == "#1e1e2e", "b16palette window = base00");
    expect(result["windowText"] == "#cdd6f4", "b16palette windowText = base05");
    expect(result["base"] == "#181825", "b16palette base = base01");
    expect(result["alternateBase"] == "#313244", "b16palette alternateBase = base02");
    expect(result["text"] == "#cdd6f4", "b16palette text = base05");
    expect(result["button"] == "#181825", "b16palette button = base01");
    expect(result["light"] == "#f5e0dc", "b16palette light = base06");
    expect(result["mid"] == "#45475a", "b16palette mid = base03");
    expect(result["toolTipBase"] == "#181825", "b16palette toolTipBase = base01");
    expect(result["toolTipText"] == "#cdd6f4", "b16palette toolTipText = base05");

    expect(theme_color::contrastRatio(result["buttonText"], result["alternateBase"]) >= 4.5,
           "b16palette buttonText readable on alternateBase");
    expect(theme_color::contrastRatio(result["link"], result["alternateBase"]) >= 4.5,
           "b16palette link readable on alternateBase");

    // Contrast check: highlightedText on highlight should be >= 4.5
    auto htCr = theme_color::contrastRatio(result["highlightedText"], result["highlight"]);
    expect(htCr >= 4.5, "b16palette highlightedText/highlight contrast >= 4.5");
}

static void
testBase16ToCustom()
{
    theme_color::Palette base16 = {
      {"base08", "#f38ba8"}, {"base09", "#fab387"}, {"base0B", "#a6e3a1"},
    };

    auto result = theme_color::base16ToCustom(base16);
    expect(result["attention"] == "#f38ba8", "b16custom attention = base08");
    expect(result["success"] == "#a6e3a1", "b16custom success = base0B");
    expect(result["warning"] == "#fab387", "b16custom warning = base09");
    expect(result["error"] == "#f38ba8", "b16custom error = base08");
}

static void
testEnsureContrast()
{
    // Deliberately poor-contrast mapping
    theme_color::Palette mapping = {
      {"window", "#222222"},          {"windowText", "#cccccc"},
      {"base", "#222222"},            {"alternateBase", "#333333"},
      {"text", "#cccccc"},            {"brightText", "#333333"},
      {"button", "#222222"},          {"buttonText", "#888888"},
      {"light", "#cccccc"},           {"mid", "#444444"},
      {"dark", "#252525"},            {"highlight", "#333333"},
      {"highlightedText", "#444444"}, {"link", "#333333"},
      {"toolTipBase", "#222222"},     {"toolTipText", "#cccccc"},
    };
    theme_color::Palette base16 = {
      {"base00", "#222222"}, {"base01", "#333333"}, {"base02", "#444444"},
      {"base03", "#555555"}, {"base04", "#888888"}, {"base05", "#cccccc"},
      {"base06", "#dddddd"}, {"base07", "#eeeeee"},
    };

    auto htBefore = theme_color::contrastRatio(mapping["highlightedText"], mapping["highlight"]);

    theme_color::ensureContrast(mapping, base16, "dark");

    auto htAfter = theme_color::contrastRatio(mapping["highlightedText"], mapping["highlight"]);
    expect(htAfter >= htBefore, "ensureContrast improves or maintains contrast");
    expect(htAfter >= 4.5, "ensureContrast highlightedText/highlight >= 4.5");
    expect(theme_color::contrastRatio(mapping["brightText"], mapping["dark"]) >= 4.5,
           "ensureContrast brightText/dark >= 4.5");
    expect(theme_color::contrastRatio(mapping["buttonText"], mapping["alternateBase"]) >= 4.5,
           "ensureContrast buttonText readable on alternateBase");
    expect(theme_color::contrastRatio(mapping["link"], mapping["alternateBase"]) >= 4.5,
           "ensureContrast link readable on alternateBase");
}

static void
testLinearizeDelinearizeRoundTrip()
{
    for (int v = 0; v <= 255; ++v) {
        int result = theme_color::delinearize(theme_color::linearize(v));
        expect(std::abs(result - v) <= 1,
               "linearize/delinearize round-trip within 1");
        if (std::abs(result - v) > 1)
            break; // avoid flooding with 255 failures
    }
}

static void
testAdjustBgForContrast()
{
    // Already sufficient contrast
    auto result = theme_color::adjustBgForContrast("#000000", "#ffffff", 4.5);
    expect(result == "#000000", "adjustBg no change when sufficient");

    // Needs adjustment
    auto result2 = theme_color::adjustBgForContrast("#888888", "#999999", 3.0);
    auto cr      = theme_color::contrastRatio("#999999", result2);
    expect(cr >= 3.0, "adjustBg achieves target contrast");
}

static void
testAdjustFgForBackgrounds()
{
    std::vector<std::string> backgrounds = {
      "#1e1e2e",
      "#181825",
      "#313244",
      "#333e59",
      "#3f252f",
    };
    auto result =
      theme_color::adjustFgForBackgrounds("#7296d1", backgrounds, "dark", 4.5);

    for (const auto &background : backgrounds) {
        expect(theme_color::contrastRatio(result, background) >= 4.5,
               "adjustFgForBackgrounds achieves target contrast");
    }
}

int
main()
{
    testParseColor();
    testRgbToHex();
    testLuminance();
    testContrastRatio();
    testBlendToward();
    testDetectVariant();
    testStripVariantSuffix();
    testBase16ToPalette();
    testBase16ToCustom();
    testEnsureContrast();
    testLinearizeDelinearizeRoundTrip();
    testAdjustBgForContrast();
    testAdjustFgForBackgrounds();

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
