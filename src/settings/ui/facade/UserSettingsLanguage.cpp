// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QStringList>

#include "settings/ui/LanguagePresentation.h"
#include "settings/ui/facade/UserSettingsPage.h"

QStringList
UserSettings::languageDropdownLabels() const
{
    return komai::settings::language::dropdownLabels();
}

int
UserSettings::languageDropdownIndex() const
{
    const auto current = uiLanguage();
    if (current.isEmpty())
        return 0;
    const int idx = komai::settings::language::availableCodes().indexOf(current);
    return idx < 0 ? 0 : idx + 1;
}

void
UserSettings::setLanguageByDropdownIndex(int index)
{
    const auto &codes = komai::settings::language::availableCodes();
    if (index <= 0) {
        setUiLanguage(QString{});
        return;
    }
    if (index <= codes.size())
        setUiLanguage(codes.at(index - 1));
}
