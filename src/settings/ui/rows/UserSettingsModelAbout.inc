    // ── About Tab ───────────────────────────────────────────────────────────

    // AboutApplicationSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Application"), nullptr, SM::SectionTitle, SM::TabAbout,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AppName
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Name"), nullptr, SM::Link, SM::TabAbout,
      aboutAppNameValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Platform
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Platform"), nullptr, SM::ReadOnlyText, SM::TabAbout,
      aboutPlatformValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // CreatedBy
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Created by"), nullptr, SM::Link, SM::TabAbout,
      aboutCreatedByValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // KomaiLicense
    { QT_TRANSLATE_NOOP("UserSettingsModel", "License"), nullptr, SM::Link, SM::TabAbout,
      aboutKomaiLicenseValue,
      nullptr, {}, {}, {}, nullptr, nullptr },

    // AboutAcknowledgementsSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Acknowledgements"), nullptr, SM::SectionTitle, SM::TabAbout,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsForkBase
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Fork base"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementForkBaseProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsMatrixSdk
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Matrix runtime (matrix-rust-sdk)"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementMatrixSdkProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsFluentIcons
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Icon set (Fluent)"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementFluentIconsProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsFontAwesome
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Icon set (Font Awesome)"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementFontAwesomeProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AcknowledgementsBoringAvatars
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Default avatars (Boring Avatars)"), nullptr, SM::Link, SM::TabAbout,
      aboutAcknowledgementBoringAvatarsProjectValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
