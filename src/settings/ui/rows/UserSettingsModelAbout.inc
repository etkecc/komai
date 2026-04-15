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
    // CreatedBy
    { QT_TR_NOOP("Created by"), nullptr, SM::Link, SM::TabAbout,
      aboutCreatedByValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // KomaiLicense
    { QT_TR_NOOP("License"), nullptr, SM::Link, SM::TabAbout,
      aboutKomaiLicenseValue,
      nullptr, {}, {}, {}, nullptr, nullptr },

    // AboutAcknowledgementsSection
    { QT_TR_NOOP("Acknowledgements"), nullptr, SM::SectionTitle, SM::TabAbout,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsForkBase
    { QT_TR_NOOP("Fork base"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementForkBaseProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsFluentIcons
    { QT_TR_NOOP("Icon set (Fluent)"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementFluentIconsProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsFontAwesome
    { QT_TR_NOOP("Icon set (Font Awesome)"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementFontAwesomeProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsBoringAvatars
    { QT_TR_NOOP("Default avatars (Boring Avatars)"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementBoringAvatarsProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
