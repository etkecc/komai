// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/LanguagePresentation.h"

#include <cstring>

#include <QCoreApplication>
#include <QDir>

namespace komai::settings::language {

namespace {

struct Entry
{
    const char *code;
    const char *nativeName;
};

// Native names match the table in docs/maintainers/translations.md. The list
// is only the source of native names — the actual dropdown is filtered to the
// languages whose .qm files are present in `:/translations`, so adding a new
// language directory under `resources/langs/` shows up automatically (with a
// fallback name based on the code) even if this map is not updated.
constexpr Entry kNativeNames[] = {
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

} // namespace

QString
nativeName(const QString &code)
{
    for (const auto &entry : kNativeNames) {
        if (code == QLatin1String(entry.code))
            return QString::fromUtf8(entry.nativeName);
    }
    return code;
}

const QStringList &
availableCodes()
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
            return nativeName(lhs).localeAwareCompare(nativeName(rhs)) < 0;
        });
        return result;
    }();
    return codes;
}

QStringList
dropdownLabels()
{
    QStringList labels;
    labels.reserve(availableCodes().size() + 1);
    labels.append(QCoreApplication::translate("UserSettingsModel", "Use system"));
    for (const auto &code : availableCodes())
        labels.append(nativeName(code));
    return labels;
}

} // namespace komai::settings::language
