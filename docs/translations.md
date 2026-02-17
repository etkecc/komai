# 🌐 Translations

Komai inherits many human-made translations from nheko — but some languages had no translations at all, and others had very low coverage. Both result in a poor experience for non-English speakers.

To address this, Komai uses AI-assisted translation (via the [Claude CLI](https://docs.anthropic.com/en/docs/claude-cli)) to fill in the gaps. We believe that properly-guided AI can produce good-enough translations, which can later be improved by human contributors if and when necessary. Having complete (even if imperfect) translations is far better than having none.


## 📂 Directory structure

Translation files live in per-language directories under [`resources/langs/`](../resources/langs/):

```
resources/langs/
  GUIDE.md                      # Instructions given to Claude for translation
  de/
    komai_de.ts                  # German translations
    GUIDE.md                     # Optional: German-specific translation instructions
  fr/
    komai_fr.ts
  ...
```

The English file (`en/komai_en.ts`) serves as the reference — it contains all source strings.


## 🔄 Updating source strings

After modifying UI strings in source code (C++ or QML), run:

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

The translation pipeline lives in [`bin/translations-translate.py`](../bin/translations-translate.py).

### ⚙️ How it works

1. The script parses a `.ts` file and extracts all `<translation type="unfinished">` entries
2. It sends batches of source strings (with their QML/C++ context names) to Claude as JSON
3. Claude returns translations following the rules in [`resources/langs/GUIDE.md`](../resources/langs/GUIDE.md)
4. The script injects translations back into the `.ts` file, removing the `type="unfinished"` attribute
5. 💾 Each batch is saved immediately — safe to abort mid-run (Ctrl+C) without losing progress
6. 🔁 Re-running the script only processes remaining unfinished strings

### 📝 Translation instructions

The file [`resources/langs/GUIDE.md`](../resources/langs/GUIDE.md) is the system prompt given to Claude. It contains rules about preserving placeholders (`%1`, `%2`), HTML tags, XML entities, keyboard shortcuts, and untranslatable terms (Matrix, Komai, etc.).

Optional per-language guides can be placed at `resources/langs/{LANGUAGE}/GUIDE.md` (e.g., `resources/langs/de/GUIDE.md` for German-specific instructions like formal/informal address). If present, they are appended to the general guide.

### 🔤 Translating a single language

```sh
just translations-claude-translate-lang de
```

Options (passed after the language code):

| Flag | Default | Description |
|------|---------|-------------|
| `--batch-size N` | 75 | Strings per Claude call |
| `--model MODEL` | CLI default | Claude model to use |
| `--dry-run` | off | Show unfinished strings without translating |

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

# 3. Review the diff, commit
git diff resources/langs/
git add resources/langs/ && git commit -m "Update translations"
```

### ⚠️ Caveats

- **Plural forms** (numerus messages like `%n file(s)`) are not yet supported by the AI translation pipeline. They require language-specific plural rules and multiple translation variants per string. The script skips them and reports the count.
- Short or ambiguous strings (single words like "Call", "State") may occasionally be skipped by Claude. Re-running picks them up since only unfinished strings are processed.
- The script requires the `claude` CLI to be installed and authenticated.
- Very large batches may hit context limits. The default batch size of 75 works well in practice.
- AI translations should be reviewed, especially for languages with complex grammar or honorific systems. Per-language `GUIDE.md` files can help steer Claude toward the right register.
