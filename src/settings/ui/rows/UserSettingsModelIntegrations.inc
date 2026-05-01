    // ── Integrations Tab ──────────────────────────────────────────────────

#ifdef KOMAI_DBUS_SYS
    // IntegrationsDbusSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "D-Bus"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsDbusApiAccess
    { QT_TRANSLATE_NOOP("UserSettingsModel", "D-Bus access"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Choose how much <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/settings/integrations/dbus.md\">D-Bus access</a> Komai exposes to local callers."),
      SM::SegmentedOptions, SM::TabIntegrations,
      integrationsDbusApiAccessValue,
      setIntegrationsDbusApiAccessValue,
      {}, {}, {},
      integrationsDbusApiAccessValues,
      nullptr,
      CORE_CONFIG_ID(IntegrationsDbusApiAccess, SettingKey::IntegrationsDbusApiAccess) },
#endif
    // IntegrationsMrsSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Matrix Rooms Search"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsMrsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Enabled"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "When enabled, adds <a href=\"https://github.com/etkecc/mrs\">Matrix Rooms Search</a> support to the <a href=\"komai://rooms-directory\">Explore Public Rooms</a> dialog."),
      SM::TabIntegrations, networkMrsEnabled, setNetworkMrsEnabled, nullptr, NetworkMrsEnabled, SettingKey::NetworkMrsEnabled),
    // IntegrationsMrsServerName
    SIMPLE_TEXT_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Server"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Any instance of <a href=\"https://github.com/etkecc/mrs\">Matrix Rooms Search</a> will do, but <a href=\"https://matrixrooms.info\">MatrixRooms.info</a> is a large popular instance run by <a href=\"https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=settings/integrations/mrs\">etke.cc</a>."),
      SM::TabIntegrations, networkMrsServerName, setNetworkMrsServerName, nullptr, NetworkMrsServerName, SettingKey::NetworkMrsServerName),

    // IntegrationsBrowserSection (control loaded via IntegrationsTab footerContent)
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Browser"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
