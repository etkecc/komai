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

#include <yaml-cpp/yaml.h>

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
    try {
        auto root = YAML::Load(content.toStdString());
        if (root["name"] && root["name"].IsScalar())
            outName = root["name"].as<std::string>();
        if (root["author"] && root["author"].IsScalar())
            outAuthor = root["author"].as<std::string>();

        YAML::Node pal;
        if (root["palette"] && root["palette"].IsMap()) {
            pal = root["palette"];
        } else {
            // Older format: base00-base0F at top level
            pal = root;
        }

        for (int i = 0; i < 16; ++i) {
            char slot[8];
            std::snprintf(slot, sizeof(slot), "base%02X", i);
            if (pal[slot] && pal[slot].IsScalar()) {
                auto val = pal[slot].as<std::string>();
                // Ensure #-prefixed hex
                if (!val.empty() && val[0] != '#')
                    val = "#" + val;
                outPalette[slot] = val;
            }
        }
    } catch (const YAML::Exception &) {
        return false;
    }
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
