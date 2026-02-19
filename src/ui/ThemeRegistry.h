// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <vector>

#include <QString>
#include <QStringList>
#include <QStringView>

#include "ThemeDefinitions.h"

class ThemeRegistry
{
public:
    static void initialize();
    static ThemeRegistry &instance();

    const ThemeDef *findTheme(QStringView slug) const;
    QStringList themeNames(const QString &variant = {}) const;
    QStringList themeSlugs(const QString &variant = {}) const;
    QString themeVariant(QStringView slug) const;
    QString defaultThemeSlug(QStringView variant) const;

private:
    ThemeRegistry();
    void loadExternalThemes();
    std::optional<ThemeDef> parseThemeFile(const QString &path, const QString &slug);

    std::vector<ThemeDef> allThemes_;
};
