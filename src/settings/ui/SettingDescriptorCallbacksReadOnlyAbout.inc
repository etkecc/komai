// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
aboutAppNameValue()
{
    return QStringLiteral("<a href=\"https://komai.chat/?utm_source=komai&utm_medium=app&utm_campaign=settings/about\">Komai</a> @ ") +
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
    return QStringLiteral(
      "<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 / "
      "<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
      "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a> "
      "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/blob/master/COPYING\">GPL-3.0-or-later</a>)");
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
aboutAcknowledgementFontAwesomeProjectValue()
{
    return QStringLiteral(
      "<a href=\"https://github.com/FortAwesome/Font-Awesome\">Font Awesome Free</a> @ ") +
           QString::fromStdString(komai::fontawesome_icons_ref) +
           QStringLiteral(" by <a href=\"https://fontawesome.com\">Fonticons, Inc.</a> "
                          "(<a href=\"https://github.com/FortAwesome/Font-Awesome/blob/6.x/LICENSE.txt\">CC BY 4.0</a>)");
}

static QVariant
aboutAcknowledgementBoringAvatarsProjectValue()
{
    return QStringLiteral(
      "<a href=\"https://github.com/boringdesigners/boring-avatars\">Boring Avatars</a>"
      " by <a href=\"https://boringdesigners.com\">Boring Designers</a> "
      "(<a href=\"https://github.com/boringdesigners/boring-avatars/blob/main/LICENSE.md\">MIT</a>)");
}

static QVariant
aboutAcknowledgementMatrixSdkProjectValue()
{
    const QString rev     = QString::fromLatin1(komai::matrix_sdk_rev);
    const QString version = QString::fromLatin1(komai::matrix_sdk_version);

    QString refLabel;
    QString refUrl;
    if (!rev.isEmpty()) {
        refLabel = rev.left(8);
        refUrl   = QStringLiteral("https://github.com/matrix-org/matrix-rust-sdk/commit/") + rev;
    } else if (!version.isEmpty()) {
        refLabel = QStringLiteral("v") + version;
        refUrl   = QStringLiteral("https://github.com/matrix-org/matrix-rust-sdk/releases/tag/matrix-sdk-") + version;
    }

    QString refSegment;
    if (!refLabel.isEmpty()) {
        refSegment = QStringLiteral(" @ <a href=\"") + refUrl + QStringLiteral("\">") + refLabel + QStringLiteral("</a>");
    }

    return QStringLiteral(
             "<a href=\"https://github.com/matrix-org/matrix-rust-sdk\">matrix-rust-sdk</a>") +
           refSegment +
           QStringLiteral(" by <a href=\"https://matrix.org\">Matrix.org Foundation</a> "
                          "(<a href=\"https://github.com/matrix-org/matrix-rust-sdk/blob/main/LICENSE\">Apache-2.0</a>)");
}

#ifdef ELEMENT_CALL_AVAILABLE
static QVariant
aboutAcknowledgementElementCallProjectValue()
{
    const QString version = QString::fromLatin1(komai::element_call_version);

    QString refSegment;
    if (!version.isEmpty() && version != QStringLiteral("unknown")) {
        refSegment = QStringLiteral(" @ <a href=\"https://github.com/element-hq/element-call/releases/tag/v") +
                     version + QStringLiteral("\">") + version + QStringLiteral("</a>");
    }

    return QStringLiteral(
             "<a href=\"https://github.com/element-hq/element-call\">Element Call</a>") +
           refSegment +
           QStringLiteral(" by <a href=\"https://element.io\">New Vector Ltd (Element)</a> "
                          "(<a href=\"https://github.com/element-hq/element-call/blob/main/LICENSE-AGPL-3.0\">"
                          "AGPL-3.0-only</a>)");
}

static QVariant
aboutAcknowledgementQtWebEngineProjectValue()
{
    return QStringLiteral(
      "<a href=\"https://doc.qt.io/qt-6/qtwebengine-overview.html\">Qt WebEngine</a> (Chromium) "
      "by <a href=\"https://www.qt.io\">The Qt Company</a> "
      "(<a href=\"https://doc.qt.io/qt-6/qtwebengine-licensing.html\">LGPL-3.0 / BSD-style</a>)");
}
#endif

static QVariant
aboutCreatedByValue()
{
    return QStringLiteral("<a href=\"https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=settings/about\">etke.cc</a>");
}

static QVariant
aboutKomaiLicenseValue()
{
    return QStringLiteral(
      "<a href=\"https://github.com/etkecc/komai/blob/main/LICENSES/GPL-3.0-or-later.txt\">"
      "GPL-3.0-or-later</a>");
}
