// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeCommandUtils.h"

#include <cctype>
#include <cstdio>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include "komai-rust-cxxbridge/lib.h"

namespace theme_command {

static void
writeUserColorSlot(QTextStream &out, const QString &indent, const theme_color::UserColorSlot &slot)
{
    out << indent << "background: \"" << QString::fromStdString(slot.background) << "\"\n";
    if (!slot.text.empty())
        out << indent << "text: \"" << QString::fromStdString(slot.text) << "\"\n";
    if (!slot.secondaryText.empty()) {
        out << indent << "secondaryText: \"" << QString::fromStdString(slot.secondaryText)
            << "\"\n";
    }
    if (!slot.link.empty())
        out << indent << "link: \"" << QString::fromStdString(slot.link) << "\"\n";
}

QString
userThemesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/komai/themes");
}

QByteArray
httpGet(QNetworkAccessManager &nam, const QUrl &url, int timeoutMs)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "komai-theme-cli/1.0");

    auto *reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        delete reply;
        return {};
    }

    auto data = reply->readAll();
    auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    delete reply;

    if (code < 200 || code >= 300)
        return {};

    return data;
}

bool
writeThemeYaml(const QString &path,
               const std::string &name,
               const std::string &author,
               const std::string &variant,
               const theme_color::Palette &palette,
               const theme_color::UserColors &userColors,
               const theme_color::Palette *sourceBase16)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "name: \"" << QString::fromStdString(name) << "\"\n";
    if (!author.empty())
        out << "author: \"" << QString::fromStdString(author) << "\"\n";
    out << "variant: \"" << QString::fromStdString(variant) << "\"\n";
    out << "palette:\n";

    for (const auto *key : theme_color::PALETTE_KEYS) {
        auto it = palette.find(key);
        if (it != palette.end())
            out << "  " << key << ": \"" << QString::fromStdString(it->second) << "\"\n";
    }
    for (const auto *key : theme_color::CUSTOM_KEYS) {
        auto it = palette.find(key);
        if (it != palette.end())
            out << "  " << key << ": \"" << QString::fromStdString(it->second) << "\"\n";
    }

    out << "userColors:\n";
    out << "  self:\n";
    writeUserColorSlot(out, QStringLiteral("    "), userColors.self);
    out << "  others:\n";
    for (const auto &slot : userColors.others) {
        out << "    - background: \"" << QString::fromStdString(slot.background) << "\"\n";
        if (!slot.text.empty())
            out << "      text: \"" << QString::fromStdString(slot.text) << "\"\n";
        if (!slot.secondaryText.empty()) {
            out << "      secondaryText: \"" << QString::fromStdString(slot.secondaryText)
                << "\"\n";
        }
        if (!slot.link.empty())
            out << "      link: \"" << QString::fromStdString(slot.link) << "\"\n";
    }

    if (sourceBase16) {
        out << "source_base16:\n";
        for (int i = 0; i < 16; ++i) {
            char slot[8];
            std::snprintf(slot, sizeof(slot), "base%02X", i);
            auto it = sourceBase16->find(slot);
            if (it != sourceBase16->end())
                out << "  " << slot << ": \"" << QString::fromStdString(it->second) << "\"\n";
        }
    }

    return true;
}

bool
parseBase16Yaml(const QByteArray &content,
                std::string &outName,
                std::string &outAuthor,
                theme_color::Palette &outPalette)
{
    const auto parsed = ::komai::rust::theme_parse_base16_yaml(content.toStdString());
    if (!parsed.has_document)
        return false;

    outName   = static_cast<std::string>(parsed.name);
    outAuthor = static_cast<std::string>(parsed.author);

    outPalette["base00"] = static_cast<std::string>(parsed.palette.base00);
    outPalette["base01"] = static_cast<std::string>(parsed.palette.base01);
    outPalette["base02"] = static_cast<std::string>(parsed.palette.base02);
    outPalette["base03"] = static_cast<std::string>(parsed.palette.base03);
    outPalette["base04"] = static_cast<std::string>(parsed.palette.base04);
    outPalette["base05"] = static_cast<std::string>(parsed.palette.base05);
    outPalette["base06"] = static_cast<std::string>(parsed.palette.base06);
    outPalette["base07"] = static_cast<std::string>(parsed.palette.base07);
    outPalette["base08"] = static_cast<std::string>(parsed.palette.base08);
    outPalette["base09"] = static_cast<std::string>(parsed.palette.base09);
    outPalette["base0A"] = static_cast<std::string>(parsed.palette.base0a);
    outPalette["base0B"] = static_cast<std::string>(parsed.palette.base0b);
    outPalette["base0C"] = static_cast<std::string>(parsed.palette.base0c);
    outPalette["base0D"] = static_cast<std::string>(parsed.palette.base0d);
    outPalette["base0E"] = static_cast<std::string>(parsed.palette.base0e);
    outPalette["base0F"] = static_cast<std::string>(parsed.palette.base0f);

    return true;
}

bool
validateBase16Palette(const theme_color::Palette &palette)
{
    for (int i = 0; i < 16; ++i) {
        char slot[8];
        std::snprintf(slot, sizeof(slot), "base%02X", i);
        auto it = palette.find(slot);
        if (it == palette.end())
            return false;
        const auto &val = it->second;
        if (val.size() != 7 || val[0] != '#')
            return false;
        for (std::size_t j = 1; j < val.size(); ++j) {
            if (!std::isxdigit(static_cast<unsigned char>(val[j])))
                return false;
        }
    }
    return true;
}

} // namespace theme_command
