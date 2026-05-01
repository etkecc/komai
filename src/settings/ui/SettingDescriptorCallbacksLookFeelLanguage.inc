// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

namespace {

struct LanguageEntry
{
    const char *code;
    const char *nativeName;
};

// Native names match the table in docs/maintainers/translations.md. The list
// is only the source of native names — the actual dropdown is filtered to the
// languages whose .qm files are present in `:/translations`, so adding a new
// language directory under `resources/langs/` shows up automatically (with a
// fallback name based on the code) even if this map is not updated.
constexpr LanguageEntry kLanguageNativeNames[] = {
    {"ar", "العربية"},
    {"bg", "Български"},
    {"ca", "Català"},
    {"cs", "Čeština"},
    {"de", "Deutsch"},
    {"el", "Ελληνικά"},
    {"en", "English"},
    {"eo", "Esperanto"},
    {"es", "Español"},
    {"et", "Eesti"},
    {"fa", "فارسی"},
    {"fi", "Suomi"},
    {"fr", "Français"},
    {"hu", "Magyar"},
    {"id", "Bahasa Indonesia"},
    {"ie", "Interlingue"},
    {"it", "Italiano"},
    {"ja", "日本語"},
    {"ko", "한국어"},
    {"ml", "മലയാളം"},
    {"nl", "Nederlands"},
    {"pl", "Polski"},
    {"pt_BR", "Português (Brasil)"},
    {"pt_PT", "Português (Portugal)"},
    {"ro", "Română"},
    {"ru", "Русский"},
    {"si", "සිංහල"},
    {"sr_Latn", "Srpski"},
    {"sv", "Svenska"},
    {"tr", "Türkçe"},
    {"uk", "Українська"},
    {"vi", "Tiếng Việt"},
    {"zh_CN", "简体中文"},
    {"zh_Hant", "繁體中文"},
};

QString
languageNativeName(const QString &code)
{
    for (const auto &entry : kLanguageNativeNames) {
        if (code == QLatin1String(entry.code))
            return QString::fromUtf8(entry.nativeName);
    }
    return code;
}

const QStringList &
availableLanguageCodes()
{
    static const QStringList codes = []() {
        QStringList result;
        QDir dir(QStringLiteral(":/translations"));
        const auto entries = dir.entryList(QStringList{QStringLiteral("komai_*.qm")}, QDir::Files);
        for (const auto &name : entries) {
            QString code = name;
            code.remove(0, static_cast<int>(std::strlen("komai_")));
            if (code.endsWith(QStringLiteral(".qm")))
                code.chop(3);
            if (!code.isEmpty())
                result.append(code);
        }
        std::sort(result.begin(), result.end(), [](const QString &lhs, const QString &rhs) {
            return languageNativeName(lhs).localeAwareCompare(languageNativeName(rhs)) < 0;
        });
        return result;
    }();
    return codes;
}

QStringList
languageDropdownLabels()
{
    QStringList labels;
    labels.reserve(availableLanguageCodes().size() + 1);
    labels.append(QCoreApplication::translate("UserSettingsModel", "Use system"));
    for (const auto &code : availableLanguageCodes())
        labels.append(languageNativeName(code));
    return labels;
}

} // namespace

static QVariant
lookFeelLanguageValue()
{
    auto i = I;
    if (!i)
        return 0;

    const auto current = i->uiLanguage();
    if (current.isEmpty())
        return 0;

    const int idx = availableLanguageCodes().indexOf(current);
    return idx < 0 ? 0 : idx + 1;
}

static bool
setLookFeelLanguageValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    const auto &codes = availableLanguageCodes();
    return applyIndexedOption(v, 0, codes.size(), [i, &codes](int idx) {
        i->setUiLanguage(idx == 0 ? QString{} : codes.at(idx - 1));
    });
}

static QVariant
lookFeelLanguageValues()
{
    return languageDropdownLabels();
}
