    // ── Look & Feel Tab ─────────────────────────────────────────────────────

    // LookFeelAppearanceSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Appearance"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Language
    // Description shares the same source string as Font size so canonicalisation
    // collapses both to a single translation per language. Drifting them apart
    // means the AI translates the shared "may only update after restart" warning
    // two different ways in the same language.
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Language"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Most UI elements update live. "
                                             "Some labels may only update after restarting the app."),
      SM::SearchableOptions, SM::TabLookFeel,
      lookFeelLanguageValue,
      setLookFeelLanguageValue,
      {}, {}, {},
      lookFeelLanguageValues,
      nullptr,
      CORE_CONFIG_ID(UiLanguage, SettingKey::UiLanguage),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "locale, translation, i18n, localization") },
    // Theme
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Theme"),
      QT_TRANSLATE_NOOP("UserSettingsModel",
        "Need more themes? Import from <a href=\"https://tinted-theming.github.io/tinted-gallery/\">Tinted Gallery</a> or <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/themes.md\">create your own</a> (restart required)."),
      SM::ThemeSelector, SM::TabLookFeel,
      lookFeelThemeValue,
      setLookFeelThemeValue,
      {}, {}, {},
      lookFeelThemeValues,
      nullptr,
      CORE_CONFIG_ID(UiThemeSlug, SettingKey::UiThemeSlug),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "colour, colours, color, colors, skin, wallpaper, look") },
    // Density
    withKeywords(
      SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Density"),
        QT_TRANSLATE_NOOP("UserSettingsModel", "Affects spacing, icon sizes, and list row heights."),
        SM::TabLookFeel,
        uiLayoutDensity,
        setUiLayoutDensity,
        UserSettings::Density,
        lookFeelDensityValues,
        nullptr,
        UiLayoutDensity,
        SettingKey::UiLayoutDensity),
      QT_TRANSLATE_NOOP("UserSettingsModel", "spacing, padding, compact, comfortable, layout")),
    // EnableUIAnimations
    withKeywords(
      SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Enable UI animations"),
        nullptr,
        SM::TabLookFeel,
        uiMotionAnimationsEnabled,
        setUiMotionAnimationsEnabled,
        nullptr,
        UiMotionAnimationsEnabled,
        SettingKey::UiMotionAnimationsEnabled),
      QT_TRANSLATE_NOOP("UserSettingsModel", "motion, reduce motion, accessibility")),
    // ScrollbarPolicy
    withKeywords(
      SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Scrollbar visibility"),
        QT_TRANSLATE_NOOP("UserSettingsModel", "Controls when scrollbars are shown across the app."),
        SM::TabLookFeel,
        uiScrollbarPolicy,
        setUiScrollbarPolicy,
        UserSettings::ScrollbarPolicy,
        lookFeelScrollbarPolicyValues,
        nullptr,
        UiScrollbarPolicy,
        SettingKey::UiScrollbarPolicy),
      QT_TRANSLATE_NOOP("UserSettingsModel", "scroll bar, scrollbar")),
    // LookFeelFontsSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Fonts & scaling"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Font
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Font family"), nullptr, SM::SearchableOptions, SM::TabLookFeel,
      lookFeelFontFamilyValue,
      setLookFeelFontFamilyValue,
      {}, {}, {},
      lookFeelFontFamilyValues,
      nullptr,
      CORE_CONFIG_ID(UiFontFamily, SettingKey::UiFontFamily),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "typeface, font") },
    withKeywords(
      SIMPLE_DOUBLE_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Font size"),
                                      QT_TRANSLATE_NOOP("UserSettingsModel", "Most UI elements update live. "
                                                 "Some labels may only update after restarting the app."),
                                      SM::TabLookFeel,
                                      uiFontSizePt,
                                      setUiFontSizePt,
                                      8.0,
                                      24.0,
                                      0.5,
                                      UiFontSizePt,
                                      SettingKey::UiFontSizePt),
      QT_TRANSLATE_NOOP("UserSettingsModel", "text size, zoom, readability")),
    // EmojiFont
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Emoji font family"), nullptr, SM::SearchableOptions, SM::TabLookFeel,
      lookFeelEmojiFontFamilyValue,
      setLookFeelEmojiFontFamilyValue,
      {}, {}, {},
      lookFeelEmojiFontFamilyValues,
      nullptr,
      CORE_CONFIG_ID(UiFontEmojiFamily, SettingKey::UiFontEmojiFamily),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "emoji typeface") },
#ifndef Q_OS_MACOS
    // ScaleFactor
    // Note: "[restart required]" is part of the translatable label here, so each row
    // ends up translated as one whole string. That's fine while only this row uses it
    // — but if a third "[restart required]" row appears, split the suffix into its
    // own QT_TRANSLATE_NOOP and concatenate at render time, otherwise the translated
    // suffix tends to drift between rows (e.g. Bulgarian "необходимо е рестартиране"
    // vs "необходимо рестартиране").
    withKeywords(
      SIMPLE_DOUBLE_CONFIG_RESTART_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Scale factor [restart required]"),
        nullptr,
        SM::TabLookFeel, uiScaleFactor, setUiScaleFactor, settings::core::kMinScaleFactor,
        settings::core::kMaxScaleFactor, settings::core::kScaleFactorStep,
        UiScaleFactor, SettingKey::UiScaleFactor),
      QT_TRANSLATE_NOOP("UserSettingsModel", "dpi, zoom, hidpi, resolution")),
#endif
    // LookFeelAvatarsSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Avatars"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AvatarPreview
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Avatar preview"),
      nullptr,
      SM::AvatarPreview, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CircularAvatarsEnabled
    withKeywords(
      SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Use circular avatars"),
                          nullptr,
                          SM::TabLookFeel, uiAvatarsCircular, setUiAvatarsCircular, nullptr, UiAvatarsCircular, SettingKey::UiAvatarsCircular),
      QT_TRANSLATE_NOOP("UserSettingsModel", "round avatars, round, roundness, circle avatars, avatar shape")),
    // DefaultAvatarStyle
    withKeywords(
      SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Default avatar style"),
        QT_TRANSLATE_NOOP("UserSettingsModel", "Shown for users and rooms that have no custom avatar."),
        SM::TabLookFeel,
        uiAvatarsDefaultAvatarStyle,
        setUiAvatarsDefaultAvatarStyle,
        UserSettings::DefaultAvatarStyle,
        lookFeelDefaultAvatarStyleValues,
        nullptr,
        UiAvatarsDefaultAvatarStyle,
        SettingKey::UiAvatarsDefaultAvatarStyle),
      QT_TRANSLATE_NOOP("UserSettingsModel", "avatar fallback, avatar placeholder, initials")),
