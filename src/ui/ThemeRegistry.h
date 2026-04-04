// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <vector>

#include <QString>
#include <QStringList>
#include <QStringView>

#include "ThemeDef.h"

class ThemeRegistry
{
public:
    static void initialize();
    static ThemeRegistry &instance();

    const ThemeDef *findTheme(QStringView slug) const;
    const std::vector<ThemeDef> &allThemes() const { return allThemes_; }
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
