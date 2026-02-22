    // ── About Tab ───────────────────────────────────────────────────────────

    // AboutApplicationSection
    { QT_TR_NOOP("APPLICATION"), nullptr, SM::SectionTitle, SM::TabAbout,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AppName
    { QT_TR_NOOP("Name"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant {
          return QStringLiteral("<a href=\"https://github.com/etkecc/komai\">Komai</a> @ ") +
                 QString::fromStdString(nheko::version) +
                 QStringLiteral(" (<a href=\"https://github.com/etkecc/komai/commit/") +
                 QString::fromStdString(nheko::commit_hash) + QStringLiteral("\">") +
                 QString::fromStdString(nheko::commit_hash) + QStringLiteral("</a>)");
      },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Platform
    { QT_TR_NOOP("Platform"), nullptr, SM::ReadOnlyText, SM::TabAbout,
      []() -> QVariant { return QString::fromStdString(nheko::build_os); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // BasedOn
    { QT_TR_NOOP("Based on"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant {
          return QStringLiteral(
            "<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 "
            "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
            "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a>)");
      },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // MaintainedBy
    { QT_TR_NOOP("Maintained by"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant { return QStringLiteral("<a href=\"https://etke.cc\">etke.cc</a>"); },
      nullptr, {}, {}, {}, nullptr, nullptr },
