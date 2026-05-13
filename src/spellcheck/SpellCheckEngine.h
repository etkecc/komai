// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

// App-global spell-checking facade (one per process). Configuration (the
// master on/off toggle + the set of enabled dictionary languages) lives in
// the per-profile `config.yml` under `composer.input.spellcheck.*` and is
// reached through UserSettings; this class only drives the Rust engine
// (komai::rust::spellcheck_*) and exposes everything the settings UI and
// the composer context menu need.
//
// The per-document squiggle is drawn by SpellChecker / SpellCheckHighlighter,
// which listen to configChanged() to rehighlight.
class SpellCheckEngine : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Master "Enable spell checking" toggle.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY configChanged)
    // Enabled dictionary languages, as normalised locale codes ("en_US",
    // "bg_BG", …), kept sorted.
    Q_PROPERTY(QStringList languages READ languages WRITE setLanguages NOTIFY configChanged)

public:
    static SpellCheckEngine *instance();

    // QML singleton entry point. Defined inline so qmltyperegistrar reliably
    // picks it up — declaring the constructor private (below) belt-and-braces
    // ensures QML cannot fall back to default-construction, which would
    // silently create a second engine and break signal/slot wiring.
    static SpellCheckEngine *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool enabled() const;
    void setEnabled(bool on);

    QStringList languages() const;
    void setLanguages(const QStringList &codes);

    // [{ code, name, builtin, enabled }] over every dictionary Komai can find
    // right now (the bundled en_US + system + per-user dirs), sorted by display
    // name. Drives the toggle list in the Spellcheck settings section.
    Q_INVOKABLE QVariantList availableDictionaries();

    // Convenience for a single toggle in the settings list.
    Q_INVOKABLE void setLanguageEnabled(const QString &code, bool on);

    // Human-readable name for a locale code, in the current UI language.
    Q_INVOKABLE QString displayName(const QString &code) const;

    // [{ languageCode, language, suggestions: [..] }] for a misspelled word —
    // empty when the word is spelled correctly. `language` is the display name;
    // the UI shows per-group headers only when there are >=2 groups.
    Q_INVOKABLE QVariantList suggestionsFor(const QString &word);

    // Permanent personal-dictionary add. Emits configChanged() so open
    // highlighters re-check.
    Q_INVOKABLE void addToDictionary(const QString &word);

Q_SIGNALS:
    void configChanged();

private:
    explicit SpellCheckEngine(QObject *parent = nullptr);
    void pushToEngine();
    QString dataDir() const;
    QStringList defaultLanguages(const QStringList &discoveredCodes) const;
    // First-run seed: if the user has no configured languages, derive a
    // sensible default from QLocale::system().uiLanguages() and write it
    // back through UserSettings so it materialises in config.yml.
    void ensureLanguagesSeeded();
};
