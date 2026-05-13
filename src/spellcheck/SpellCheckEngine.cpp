// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SpellCheckEngine.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>

#include "komai-rust-cxxbridge/ffi.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
SpellCheckEngine *s_instance = nullptr;

::rust::Str
toRustStr(const QByteArray &bytes)
{
    return ::rust::Str(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString
qstr(const ::rust::String &s)
{
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

// Normalise a BCP-47-ish tag / dictionary stem to the conventional `ll` or
// `ll_CC` form (mirrors the Rust side's normalize_code so QLocale tags match
// discovered dictionary codes).
QString
normalizeCode(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return {};
    const qsizetype sep = trimmed.indexOf(QRegularExpression(QStringLiteral("[_-]")));
    QString lang        = (sep < 0 ? trimmed : trimmed.left(sep)).toLower();
    if (lang.isEmpty())
        return {};
    for (const QChar &c : lang)
        if (!c.isLetter())
            return {};
    if (sep < 0)
        return lang;
    QString rest         = trimmed.mid(sep + 1);
    const qsizetype sep2 = rest.indexOf(QRegularExpression(QStringLiteral("[_-]")));
    QString region       = (sep2 < 0 ? rest : rest.left(sep2));
    if (region.isEmpty())
        return lang;
    for (const QChar &c : region)
        if (!c.isLetterOrNumber())
            return lang;
    return lang + QStringLiteral("_") + region.toUpper();
}

// Just the language ("English", "Bulgarian"), no territory.
QString
languageNameForCode(const QString &code)
{
    const QLocale loc(code);
    const QString name = QLocale::languageToString(loc.language());
    if (name.isEmpty() || loc.language() == QLocale::AnyLanguage || loc.language() == QLocale::C)
        return code;
    return name;
}

// "English / United States", "Bulgarian / Bulgaria", "Esperanto" (no slash
// when the code carries no territory). Used everywhere a dictionary is shown
// to the user — the settings list, the spelling-suggestions submenu — so
// regional variants stay disambiguated by default (e.g. en_US vs en_GB
// produce visually distinct submenus when both are enabled).
QString
displayNameForCode(const QString &code)
{
    const QString name = languageNameForCode(code);
    if (name == code)
        return code; // unknown — show the raw code
    if (code.contains(QLatin1Char('_'))) {
        const QString territory = QLocale::territoryToString(QLocale(code).territory());
        if (!territory.isEmpty())
            return name + QStringLiteral(" / ") + territory;
    }
    return name;
}

// Return the input codes deduplicated, normalised, and alphabetically
// sorted — the canonical on-disk form for `composer.input.spellcheck.languages`.
QStringList
canonicalLanguages(const QStringList &codes)
{
    QStringList out;
    for (const QString &c : codes) {
        const QString n = normalizeCode(c);
        if (!n.isEmpty() && !out.contains(n))
            out << n;
    }
    out.sort();
    return out;
}
} // namespace

SpellCheckEngine::SpellCheckEngine(QObject *parent)
  : QObject(parent)
{
    s_instance = this;

    // Register the bundled en_US dictionary (the always-available fallback).
    QFile aff(QStringLiteral(":/dictionaries/en_US.aff"));
    QFile dic(QStringLiteral(":/dictionaries/en_US.dic"));
    if (aff.open(QIODevice::ReadOnly) && dic.open(QIODevice::ReadOnly)) {
        const QByteArray affBytes = aff.readAll();
        const QByteArray dicBytes = dic.readAll();
        komai::rust::spellcheck_register_builtin_dictionary(
          ::rust::Str("en_US", 5), toRustStr(affBytes), toRustStr(dicBytes));
    }

    ensureLanguagesSeeded();

    if (auto settings = UserSettings::instance()) {
        connect(
          settings.get(), &UserSettings::composerInputSpellcheckEnabledChanged, this, [this](bool) {
              pushToEngine();
              emit configChanged();
          });
        connect(settings.get(),
                &UserSettings::composerInputSpellcheckLanguagesChanged,
                this,
                [this](const QStringList &) {
                    pushToEngine();
                    emit configChanged();
                });
    }

    pushToEngine();
}

SpellCheckEngine *
SpellCheckEngine::instance()
{
    if (!s_instance)
        new SpellCheckEngine();
    return s_instance;
}

QString
SpellCheckEngine::dataDir() const
{
    // Komai's shared data root (~/.local/share/komai on Linux, the equivalent
    // elsewhere) — same place `themes/` lives. The personal word list and any
    // hand-installed dictionaries are "things this human uses", not per-account,
    // so they live here rather than under the per-profile config dir. Under it
    // we keep `personal.dic` and a `hunspell/` dir that's one of the dictionary
    // search locations.
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.local/share");
    const QString dir = base + QStringLiteral("/komai");
    QDir().mkpath(dir);
    return dir;
}

bool
SpellCheckEngine::enabled() const
{
    if (auto settings = UserSettings::instance())
        return settings->composerInputSpellcheckEnabled();
    return true;
}

QStringList
SpellCheckEngine::languages() const
{
    auto settings = UserSettings::instance();
    if (!settings)
        return {};
    return canonicalLanguages(settings->composerInputSpellcheckLanguages());
}

void
SpellCheckEngine::ensureLanguagesSeeded()
{
    auto settings = UserSettings::instance();
    if (!settings)
        return;
    if (!settings->composerInputSpellcheckLanguages().isEmpty())
        return;
    QStringList discovered;
    for (const auto &d : komai::rust::spellcheck_discover_dictionaries())
        discovered << qstr(d.code);
    settings->setComposerInputSpellcheckLanguages(canonicalLanguages(defaultLanguages(discovered)));
}

QStringList
SpellCheckEngine::defaultLanguages(const QStringList &discoveredCodes) const
{
    QStringList result;
    const QStringList prefs = QLocale::system().uiLanguages();
    for (const QString &pref : prefs) {
        const QString norm = normalizeCode(pref);
        if (norm.isEmpty())
            continue;
        if (discoveredCodes.contains(norm)) {
            if (!result.contains(norm))
                result << norm;
            continue;
        }
        const QString lang = norm.section(QLatin1Char('_'), 0, 0);
        for (const QString &c : discoveredCodes) {
            if (c == lang || c.startsWith(lang + QLatin1Char('_'))) {
                if (!result.contains(c))
                    result << c;
                break;
            }
        }
    }
    if (result.isEmpty()) {
        result << QStringLiteral("en_US"); // bundled backstop
    } else {
        const bool hasEnglish = std::any_of(result.cbegin(), result.cend(), [](const QString &c) {
            return c == QStringLiteral("en") || c.startsWith(QStringLiteral("en_"));
        });
        if (!hasEnglish)
            result << QStringLiteral("en_US"); // English for the inevitable loanwords
    }
    result.sort();
    return result;
}

void
SpellCheckEngine::pushToEngine()
{
    ::rust::Vec<::rust::String> codes;
    for (const QString &c : languages())
        codes.push_back(::rust::String(c.toStdString()));
    const QByteArray dir = dataDir().toUtf8();
    komai::rust::spellcheck_set_config(toRustStr(dir), enabled(), codes);
}

void
SpellCheckEngine::setEnabled(bool on)
{
    if (auto settings = UserSettings::instance())
        settings->setComposerInputSpellcheckEnabled(on);
}

void
SpellCheckEngine::setLanguages(const QStringList &codes)
{
    auto settings = UserSettings::instance();
    if (!settings)
        return;
    settings->setComposerInputSpellcheckLanguages(canonicalLanguages(codes));
}

void
SpellCheckEngine::setLanguageEnabled(const QString &code, bool on)
{
    const QString n = normalizeCode(code);
    if (n.isEmpty())
        return;
    QStringList next = languages();
    if (on) {
        if (!next.contains(n))
            next << n;
    } else {
        next.removeAll(n);
    }
    setLanguages(next);
}

QString
SpellCheckEngine::displayName(const QString &code) const
{
    return displayNameForCode(normalizeCode(code));
}

QVariantList
SpellCheckEngine::availableDictionaries()
{
    QVariantList out;
    const QStringList active = languages();
    const QSet<QString> enabledSet(active.cbegin(), active.cend());
    for (const auto &d : komai::rust::spellcheck_discover_dictionaries()) {
        const QString code = qstr(d.code);
        QVariantMap m;
        m.insert(QStringLiteral("code"), code);
        m.insert(QStringLiteral("name"), displayNameForCode(code));
        m.insert(QStringLiteral("builtin"), d.builtin);
        m.insert(QStringLiteral("enabled"), enabledSet.contains(code));
        out.push_back(m);
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(out.begin(), out.end(), [&collator](const QVariant &a, const QVariant &b) {
        return collator.compare(a.toMap().value(QStringLiteral("name")).toString(),
                                b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    return out;
}

QVariantList
SpellCheckEngine::suggestionsFor(const QString &word)
{
    QVariantList out;
    const QByteArray utf8 = word.toUtf8();
    for (const auto &group : komai::rust::spellcheck_suggest(toRustStr(utf8))) {
        const QString code = qstr(group.language_code);
        QStringList words;
        for (const auto &s : group.suggestions)
            words << qstr(s);
        QVariantMap m;
        m.insert(QStringLiteral("languageCode"), code);
        m.insert(QStringLiteral("language"), displayNameForCode(code));
        m.insert(QStringLiteral("suggestions"), words);
        out.push_back(m);
    }
    return out;
}

void
SpellCheckEngine::addToDictionary(const QString &word)
{
    const QByteArray utf8 = word.trimmed().toUtf8();
    if (utf8.isEmpty())
        return;
    komai::rust::spellcheck_add_word(toRustStr(utf8));
    emit configChanged();
}
