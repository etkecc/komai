# 🌐 Translations

Komai inherits human-made translations from nheko, then uses AI to fill the gaps. A complete-but-imperfect translation beats no translation at all — but AI output is a starting point, not the final word. Human review and tweaks are genuinely valuable, and we'd love your help.


## 🌍 Available translations

Komai currently ships translations for the following 33 languages (plus English as the source). Click a row to jump to that language's translation files. Flags indicate broad regions only — they're a navigational aid, not a claim that a language belongs to any one country.

Users pick from this list in-app under **Settings → Look & Feel → Appearance → Language**. The default is *Use system*, which falls back to the host locale (`LANG`/`QLocale`). Picking an explicit language stores `ui.language` in `config.yml` and requires a restart to take effect.

| Flag | Language | Native name | Files |
|------|----------|-------------|-------|
| 🌐 | Arabic | العربية | [`resources/langs/ar/`](../../resources/langs/ar/) |
| 🇧🇬 | Bulgarian | Български | [`resources/langs/bg/`](../../resources/langs/bg/) |
| 🏴 | Catalan | Català | [`resources/langs/ca/`](../../resources/langs/ca/) |
| 🇨🇳 | Chinese (Simplified) | 简体中文 | [`resources/langs/zh_CN/`](../../resources/langs/zh_CN/) |
| 🇹🇼 | Chinese (Traditional) | 繁體中文 | [`resources/langs/zh_Hant/`](../../resources/langs/zh_Hant/) |
| 🇨🇿 | Czech | Čeština | [`resources/langs/cs/`](../../resources/langs/cs/) |
| 🇳🇱 | Dutch | Nederlands | [`resources/langs/nl/`](../../resources/langs/nl/) |
| 🌐 | Esperanto | Esperanto | [`resources/langs/eo/`](../../resources/langs/eo/) |
| 🇪🇪 | Estonian | Eesti | [`resources/langs/et/`](../../resources/langs/et/) |
| 🇫🇮 | Finnish | Suomi | [`resources/langs/fi/`](../../resources/langs/fi/) |
| 🇫🇷 | French | Français | [`resources/langs/fr/`](../../resources/langs/fr/) |
| 🇩🇪 | German | Deutsch | [`resources/langs/de/`](../../resources/langs/de/) |
| 🇬🇷 | Greek | Ελληνικά | [`resources/langs/el/`](../../resources/langs/el/) |
| 🇭🇺 | Hungarian | Magyar | [`resources/langs/hu/`](../../resources/langs/hu/) |
| 🇮🇩 | Indonesian | Bahasa Indonesia | [`resources/langs/id/`](../../resources/langs/id/) |
| 🌐 | Interlingue | Interlingue | [`resources/langs/ie/`](../../resources/langs/ie/) |
| 🇮🇹 | Italian | Italiano | [`resources/langs/it/`](../../resources/langs/it/) |
| 🇯🇵 | Japanese | 日本語 | [`resources/langs/ja/`](../../resources/langs/ja/) |
| 🇰🇷 | Korean | 한국어 | [`resources/langs/ko/`](../../resources/langs/ko/) |
| 🇮🇳 | Malayalam | മലയാളം | [`resources/langs/ml/`](../../resources/langs/ml/) |
| 🇮🇷 | Persian | فارسی | [`resources/langs/fa/`](../../resources/langs/fa/) |
| 🇵🇱 | Polish | Polski | [`resources/langs/pl/`](../../resources/langs/pl/) |
| 🇧🇷 | Portuguese (Brazil) | Português (Brasil) | [`resources/langs/pt_BR/`](../../resources/langs/pt_BR/) |
| 🇵🇹 | Portuguese (Portugal) | Português (Portugal) | [`resources/langs/pt_PT/`](../../resources/langs/pt_PT/) |
| 🇷🇴 | Romanian | Română | [`resources/langs/ro/`](../../resources/langs/ro/) |
| 🇷🇺 | Russian | Русский | [`resources/langs/ru/`](../../resources/langs/ru/) |
| 🇷🇸 | Serbian (Latin) | Srpski | [`resources/langs/sr_Latn/`](../../resources/langs/sr_Latn/) |
| 🇱🇰 | Sinhala | සිංහල | [`resources/langs/si/`](../../resources/langs/si/) |
| 🇪🇸 | Spanish | Español | [`resources/langs/es/`](../../resources/langs/es/) |
| 🇸🇪 | Swedish | Svenska | [`resources/langs/sv/`](../../resources/langs/sv/) |
| 🇹🇷 | Turkish | Türkçe | [`resources/langs/tr/`](../../resources/langs/tr/) |
| 🇺🇦 | Ukrainian | Українська | [`resources/langs/uk/`](../../resources/langs/uk/) |
| 🇻🇳 | Vietnamese | Tiếng Việt | [`resources/langs/vi/`](../../resources/langs/vi/) |

## 🙋 Contributing fixes

**Spotted a translation that's wrong, awkward, or just off?** Please open a pull request.

No signup for translation platforms, no CLA, no gatekeeping — just edit the file and send a PR. Even one-line tweaks are welcome. If you'd rather describe the issue than fix it yourself, a [GitHub issue](https://github.com/etkecc/komai/issues) is fine too.

The two files you might want to touch:

- The translations themselves: `resources/langs/<lang>/komai_<lang>.ts`
- The per-language style guide that steers the AI: `resources/langs/<lang>/GUIDE.md` — improvements here help every future re-run, not just the strings you fix today

### Things to watch for

AI gets the literal meaning right most of the time, but tends to miss what a native speaker would catch:

- **Gender mismatches** in languages that gender verbs or adjectives by speaker/addressee
- **Unnatural phrasing** — grammatically correct, but nobody would actually say it that way
- **Length problems** — translations that overflow buttons, tabs, tooltips, or other tight UI
- **Inconsistent register** — formal and casual mixed within one screen, or shifting tone between related strings
- **Style drift** — different translations of the same term across the app (`translations-canonicalize` helps, but only picks among existing variants)
- **Diacritics and script mixing** — easy to miss in long files
- **Thin per-language `GUIDE.md`** — when the guide is underspecified, the model has to guess

Adding a new language or maintaining one end-to-end? The AI pipeline and tooling are documented below.


## 📂 Directory structure

Translation files live in per-language directories under [`resources/langs/`](../../resources/langs/):

```
resources/langs/
  GUIDE.md                      # Instructions given to the LLM for translation
  de/
    komai_de.ts                  # German translations
    GUIDE.md                     # Optional: German-specific translation instructions
  fr/
    komai_fr.ts
  ...
```

The English file (`en/komai_en.ts`) serves as the reference — it contains all source strings.


## 🔄 Updating source strings

After modifying UI strings in source code (C++, QML, or Rust-originated strings in C++ translation modules), run:

```sh
just translations-update
```

This calls Qt's `lupdate` to scan `src/` and `resources/qml/` and update all `.ts` files with new, changed, or removed strings, then normalizes the XML format. Newly added strings appear as `type="unfinished"` in each language file.

### XML normalization

The `.ts` files are kept in a canonical XML format produced by ElementTree. This ensures that the translation script and `lupdate` don't fight over formatting. Normalization is run automatically as part of `just translations-update`, but can also be run standalone:

```sh
just translations-normalize            # all languages
just translations-normalize --lang de  # single language
```


## 🤖 AI-powered translation

The translation pipeline lives in [`bin/translations/translate.py`](../../bin/translations/translate.py). It's provider-neutral in principle; the current backend is the [Claude CLI](https://docs.anthropic.com/en/docs/claude-cli), wired up via `call_claude()`. Swap that one function to use a different LLM.

### ⚙️ How it works

1. The script parses a `.ts` file and extracts all `<translation type="unfinished">` entries, along with their context, source-file location, and any translator comments (`comment` / `extracomment`).
2. It sorts unfinished entries by `(context, source)` so related strings batch together, giving the model consistency pressure within a single call.
3. It sends batches of source strings as JSON to the configured LLM.
4. The LLM returns translations following the rules in [`resources/langs/GUIDE.md`](../../resources/langs/GUIDE.md).
5. A post-translation validator checks that placeholders (`%1`/`%n`/`%L1`), HTML-tag counts, `&&` literal ampersands, and keyboard shortcuts are preserved. Failing translations are **rejected** and remain `unfinished` — the next run will retry them.
6. The script injects the validated translations back into the `.ts` file, removing the `type="unfinished"` attribute.
7. 💾 Each batch is saved immediately — safe to abort mid-run (Ctrl+C) without losing progress.
8. 🔁 Re-running the script only processes remaining unfinished strings.

### 📝 Translation instructions

The file [`resources/langs/GUIDE.md`](../../resources/langs/GUIDE.md) is the prompt given to the LLM. It contains rules about preserving placeholders (`%1`, `%2`, `%L1`), HTML tags, XML entities, keyboard shortcuts, Qt mnemonic accelerators (`&File` / `&&`), Matrix vocabulary consistency, quote style (ASCII-preferred for safe JSON framing), and untranslatable terms (Matrix, Komai, etc.).

Per-language guides live at `resources/langs/{LANGUAGE}/GUIDE.md` (e.g., `resources/langs/de/GUIDE.md` for German). They are appended to the general guide when translating that language. Every language Komai ships has one.

When you add a new language, write a per-language guide before running the AI translator — even a short one improves output quality dramatically. For languages with existing translations, the most useful guides are **mined from the corpus**: sample 100-300 already-translated entries, identify register, vocabulary, and typography conventions in actual use, and document them. For languages without prior translations, write a minimal register/tone primer and let the model infer the rest from the common GUIDE.

### 🔤 Translating a single language

```sh
just translations-claude-translate-lang de
```

Options (passed after the language code):

| Flag | Default | Description |
|------|---------|-------------|
| `--batch-size N` | 75 | Strings per LLM call |
| `--model MODEL` | `sonnet` | Model to use — see below |
| `--dry-run` | off | Show unfinished strings without translating |
| `--print-prompt` | off | Print the rendered prompt for the first batch and exit (debug aid) |

### Model choice

The pipeline defaults to **Sonnet** because translation is a high-volume structured-output task: Sonnet is roughly 3-5× faster than Opus per batch, noticeably cheaper, and equally reliable at JSON framing. Opus's extra reasoning adds no translation-quality gain for typical UI strings. Use `--model opus` for languages where you want to trade speed for maximum fluency (e.g., a final polish pass on a high-traffic language), or `--model haiku` for maximum throughput when quality can tolerate a small hit.

Example with options:

```sh
just translations-claude-translate-lang ja --batch-size 50 --dry-run
```

### 🌍 Translating all languages

```sh
just translations-claude-translate-all
```

This iterates over every language directory (skipping `en`), calling `translations-claude-translate-lang` for each. Failures for one language don't stop the others.

The same options can be passed:

```sh
just translations-claude-translate-all --batch-size 50
```

### 🚀 Typical workflow

```sh
# 1. Update source strings after code changes
just translations-update

# 2. Translate all languages
just translations-claude-translate-all

# 3. Pick one canonical translation per source string (optional but recommended)
just translations-canonicalize

# 4. Review the diff, commit
git diff resources/langs/
git add resources/langs/ && git commit -m "Update translations"
```

### 🧹 Canonicalizing inconsistencies

```sh
just translations-canonicalize             # all languages
just translations-canonicalize --lang de   # single language
just translations-canonicalize --dry-run   # preview without writing
```

The same source string can appear in many `<context>` blocks (different .qml files, different message classes). The AI pipeline translates each occurrence independently and may pick slightly different variants ("delete" vs "remove", "settings" vs "preferences"). [`bin/translations/normalize-inconsistencies.py`](../../bin/translations/normalize-inconsistencies.py) picks one canonical translation per `(language, source)` pair and propagates it across all occurrences, breaking ties by per-language `GUIDE.md` preference and then by frequency. It only chooses among existing translations — it never alters semantics.

### 🛡️ Drift hook

A pre-commit hook (`bin/prek/translations-drift.py`) re-runs `lupdate` against the source tree and compares the result against the committed `.ts` files. Any source change that adds, removes, or moves a translatable string fails the hook, with the suggested fix being `just translations-update`. This prevents `.ts` drift from accumulating silently between translation runs and keeps every PR's translation impact visible. The hook needs Qt's `lupdate` on the system — same assumption as the rest of the build.

### ⚠️ Caveats

- **Rust-originated strings** (timeline state events, event type labels, errors, etc.) are translated on the C++ side via dedicated translation modules. When adding or changing these strings in Rust, update the corresponding `tr()` call in the appropriate C++ module and run `just translations-update`. See [architecture/translations.md](../architecture/translations.md) for details.
- **Plural forms** (numerus messages like `%n file(s)`) are translated by a separate pass that runs alongside the regular pass — Qt's `lupdate` already encodes per-language CLDR plural-form counts, so the pipeline just fills the existing `<numerusform/>` slots in CLDR canonical order. Per-language `GUIDE.md` files document the rules per slot. Use `--regular-only` or `--numerus-only` to scope a single language run.
- Short or ambiguous strings (single words like "Call", "State") may occasionally be skipped by the model. Re-running picks them up since only unfinished strings are processed. The common `GUIDE.md` explicitly tells the model not to skip, but honours this imperfectly — which is why the script also logs specific skipped sources so you can iterate on the prompt.
- The validator is intentionally lenient: only placeholder, HTML-tag, `&&`, and keyboard-shortcut preservation are enforced. Cosmetic differences (quote style, whitespace) are not. Tune `validate_translation()` in `bin/translations/translate.py` if false positives or false negatives surface.
- The script requires the configured LLM CLI to be installed and authenticated (currently `claude`).
- Very large batches may hit context limits or per-batch CLI timeouts. The default batch size of 75 works well for Latin-script languages; languages with verbose scripts (e.g. Sinhala, Devanagari) can occasionally exceed the per-call timeout — drop to `--batch-size 30` for those if you see timeouts.
- Translatable strings must not contain inline HTML markup with `%1` substitution (`qsTr("Read the <a href=\"%1\">guide</a>.").arg(url)`). The model frequently mangles the escaped attributes and the validator can't always catch it. Split the string into a non-HTML body and a non-HTML link CTA, then concatenate with QML-side `<a href="...">` markup — see the `RoomInfoAboutTab` and `SelectionModeHelpDialog` patterns for examples.
- AI translations should be reviewed, especially for languages with complex grammar or honorific systems. Per-language `GUIDE.md` files can help steer the model toward the right register.

For technical details on why the system works the way it does (XML normalization, ElementTree vs regex, the numerus pipeline), see [architecture/translations.md](../architecture/translations.md).
