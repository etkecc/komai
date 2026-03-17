# Theme Design Guide

This document is for Komai maintainers working on built-in themes and theme-sensitive UI components.
It complements [Themes Architecture](themes.md), which covers the import/build/runtime pipeline.


## Goals

- Keep Komai themes readable at default font sizes.
- Preserve clear state changes for hover, selection, focus, warnings, and dialogs.
- Allow imported themes to keep their identity while still meeting Komai's usability floor.
- Avoid relying on a desktop/system palette to provide safe contrast in all cases.


## Palette Roles In Practice

These are the roles that matter most in the current QML, based on common usage:

- `window`:
  main page background, shell surfaces, room list row idle background
- `alternateBase`:
  sidebar background, button background, dialog background, read-only surface, list/card-like surface
- `base`:
  editable input background, timeline/background-adjacent content surface
- `text`:
  primary body text and labels
- `buttonText`:
  helper text, subtitles, timestamps, secondary labels, placeholders
- `dark`:
  hover background and active-hover surface
- `brightText`:
  text shown on `dark` hover surfaces
- `highlight`:
  selected row/button background, accent surface, focus/selection color
- `highlightedText`:
  text shown on `highlight`
- `userColors`:
  literal user/bubble slots; bubble `background` is authored directly, while optional per-slot `text`, `secondaryText`, and `link` can override the global palette for that bubble only
- `mid`:
  separator/border tone via `Komai.theme.separator`
- semantic colors `attention`, `warning`, `success`, `error`:
  inline status text, validation states, badges, icon accents

Because Komai uses `brightText` and `highlightedText` for ordinary UI labels and secondary text, these are not "decorative" pairs. They need normal-text readability.


## Contrast Targets

Use WCAG contrast ratios as the baseline. For the standard itself, see the W3C explanation of [WCAG 2.2 Success Criterion 1.4.3, Contrast (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html).

- `text` on `window`, `base`, `alternateBase`: target `>= 4.5`
- `buttonText` on `window`, `base`, `alternateBase`: target `>= 4.5`
- `highlightedText` on `highlight`: target `>= 4.5`
- `brightText` on `dark`: target `>= 4.5`
- semantic text colors used as actual text on neutral backgrounds: target `>= 4.5`
- links on neutral backgrounds: target `>= 4.5`
- bubble `text` / `secondaryText` / `link` on bubble `background`: target `>= 4.5`

For non-text surface separation:

- passive surface distinction such as `window` vs `alternateBase`: aim for roughly `>= 1.2`, prefer `>= 1.5` where practical
- borders, outlines, and state indicators that carry meaning on their own: target `>= 3.0`

Notes:

- `3.0` is acceptable for large text, but Komai often uses these colors for ordinary labels and previews.
- A theme can still feel subtle without pushing `window` and `alternateBase` far apart, but dialogs and hover/selection states need explicit separation somewhere, whether from fill, border, shadow, or all three.
- Built-in themes are expected to pass the strict AA audit for the common text pairs above. This is enforced in `prek` via the built-in theme contrast hook.


## Current UI Pairings To Protect

These pairings are particularly important because they appear in common screens:

- room list idle rows: `window` + `text` / `buttonText`
- room list hover rows: `dark` + `brightText`
- room list selected rows: `highlight` + `highlightedText`
- settings rows hover state: `dark` + `brightText`
- primary buttons: `highlight` + `highlightedText`
- normal buttons: `alternateBase` + `text`
- inputs and text areas: `base` + `text`, placeholder `buttonText`
- dialogs/popups: currently often `alternateBase` + `text`, usually above `window`


## Theme Authoring Rules

- Do not assume white text works on a bright accent color. Check it.
- Do not assume a Qt role name matches how Komai uses it. For example, `alternateBase` is not a niche table-row color here.
- Treat `mid` as a real UI border/separator color, not a leftover palette slot.
- Treat `userColors` as the actual bubble slot values in bubble style. `background` should be literal. Only add per-slot `text`, `secondaryText`, or `link` when the global palette is not good enough on that bubble.
- Treat `secondaryText` as an advanced in-bubble secondary/inactive role. Outer timeline metadata should follow the normal timeline palette unless a component explicitly chooses otherwise.
- Imported/generated `userColors` should already be the final bubble-fill values written to YAML. If a theme needs softer bubbles, soften the authored/imported color itself instead of adding runtime tinting back into QML.
- Keep the fallback model simple: slot `text` falls back to `palette.text`, slot `secondaryText` to `palette.buttonText`, and slot `link` to `palette.link`.
- For imported themes, prefer minimal tuning, but do tune when a role is used in a clearly different way in Komai than in the upstream source.
- When a role would need heavy distortion to satisfy Komai requirements, prefer adding a Komai-specific theme or a component-level fallback instead of aggressively mutating a known upstream theme.


## Component Guardrails

Theme files alone are not enough.

- Theme-sensitive controls should still have runtime contrast guardrails where necessary, especially when user-supplied themes bypass Komai's import heuristics.
- Dialogs should not rely only on `window` vs `alternateBase` fill contrast. They should also have a visible outline and/or shadow.
- Focus and selection indicators should remain perceivable even in light themes with warm accent colors.


## Workflow

When changing a built-in theme:

1. Check the common text/surface pairs above, not only the raw palette.
2. Run the theme preview SPA and scroll the affected screens across all built-in themes.
3. Verify the affected components in room list, settings, dialogs, and auth/onboarding.
4. Re-check hand-crafted external themes if the change is really a component-level safeguard.
5. Run `just lint`.

Useful local checks:

```sh
python3 bin/theme/check.py
python3 bin/theme/contrast.py komai-light
just theme-check-contrast komai-light
just theme-preview-run
just lint
```
