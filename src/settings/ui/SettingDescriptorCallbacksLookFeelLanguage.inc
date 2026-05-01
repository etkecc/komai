// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Language list, native-name table and dropdown-label helper live in
// settings/ui/LanguagePresentation.{h,cpp} so the Welcome page can build
// the same picker without duplicating the data.

static QVariant
lookFeelLanguageValue()
{
    auto i = I;
    if (!i)
        return 0;

    const auto current = i->uiLanguage();
    if (current.isEmpty())
        return 0;

    const int idx = komai::settings::language::availableCodes().indexOf(current);
    return idx < 0 ? 0 : idx + 1;
}

static bool
setLookFeelLanguageValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    const auto &codes = komai::settings::language::availableCodes();
    return applyIndexedOption(v, 0, codes.size(), [i, &codes](int idx) {
        i->setUiLanguage(idx == 0 ? QString{} : codes.at(idx - 1));
    });
}

static QVariant
lookFeelLanguageValues()
{
    return komai::settings::language::dropdownLabels();
}
