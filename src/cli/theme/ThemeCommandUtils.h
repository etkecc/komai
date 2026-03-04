// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QString>
#include <QUrl>

#include "ThemeColorUtils.h"

namespace theme_command {

QString
userThemesDir();

QByteArray
httpGet(QNetworkAccessManager &nam, const QUrl &url, int timeoutMs = 15000);

bool
writeThemeYaml(const QString &path,
               const std::string &name,
               const std::string &author,
               const std::string &variant,
               const theme_color::Palette &palette,
               const theme_color::UserColors &userColors,
               const theme_color::Palette *sourceBase16 = nullptr);

bool
parseBase16Yaml(const QByteArray &content,
                std::string &outName,
                std::string &outAuthor,
                theme_color::Palette &outPalette);

bool
validateBase16Palette(const theme_color::Palette &palette);

} // namespace theme_command
