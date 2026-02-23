    // ── About Tab ───────────────────────────────────────────────────────────

    // AboutApplicationSection
    { QT_TR_NOOP("Application"), nullptr, SM::SectionTitle, SM::TabAbout,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AppName
    { QT_TR_NOOP("Name"), nullptr, SM::Link, SM::TabAbout,
      aboutAppNameValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Platform
    { QT_TR_NOOP("Platform"), nullptr, SM::ReadOnlyText, SM::TabAbout,
      aboutPlatformValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // BasedOn
    { QT_TR_NOOP("Based on"), nullptr, SM::Link, SM::TabAbout,
      aboutBasedOnValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // MaintainedBy
    { QT_TR_NOOP("Maintained by"), nullptr, SM::Link, SM::TabAbout,
      aboutMaintainedByValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
