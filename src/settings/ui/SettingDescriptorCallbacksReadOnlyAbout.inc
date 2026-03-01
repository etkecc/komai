// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
aboutAppNameValue()
{
    return QStringLiteral("<a href=\"https://github.com/etkecc/komai\">Komai</a> @ ") +
           QString::fromStdString(komai::version) +
           QStringLiteral(" (<a href=\"https://github.com/etkecc/komai/commit/") +
           QString::fromStdString(komai::commit_hash) + QStringLiteral("\">") +
           QString::fromStdString(komai::commit_hash) + QStringLiteral("</a>)");
}

static QVariant
aboutPlatformValue()
{
    return QString::fromStdString(komai::build_os);
}

static QVariant
aboutAcknowledgementForkBaseProjectValue()
{
    return QStringLiteral("<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 "
                          "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
                          "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a>)");
}

static QVariant
aboutAcknowledgementFluentIconsProjectValue()
{
    return QStringLiteral(
      "<a href=\"https://github.com/microsoft/fluentui-system-icons\">Fluent UI System Icons</a> @ ") +
           QString::fromStdString(komai::fluent_icons_ref) +
           QStringLiteral(" by <a href=\"https://www.microsoft.com\">Microsoft</a> "
                          "(<a href=\"https://github.com/microsoft/fluentui-system-icons/blob/main/LICENSE\">MIT</a>)");
}

static QVariant
aboutMaintainedByValue()
{
    return QStringLiteral("<a href=\"https://etke.cc\">etke.cc</a>");
}

static QVariant
aboutKomaiLicenseValue()
{
    return QStringLiteral(
      "<a href=\"https://github.com/etkecc/komai/blob/main/LICENSES/GPL-3.0-or-later.txt\">"
      "GPL-3.0-or-later</a>");
}
