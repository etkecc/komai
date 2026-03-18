    // ── Integrations Tab ──────────────────────────────────────────────────

    // IntegrationsSystemTraySection
    { QT_TR_NOOP("System tray"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsTray
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Minimize to tray",
      QT_TR_NOOP("Keep the application running in the background after closing the client window."),
      SM::TabIntegrations, integrationsSystemTrayEnabled, setIntegrationsSystemTrayEnabled, nullptr, IntegrationsSystemTrayEnabled, SettingKey::IntegrationsSystemTrayEnabled),
    // IntegrationsStartInTray
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Start in tray",
      QT_TR_NOOP("Start the application in the background without showing the client window."),
      SM::TabIntegrations, integrationsSystemTrayAutostart, setIntegrationsSystemTrayAutostart, nullptr, IntegrationsSystemTrayAutostart, SettingKey::IntegrationsSystemTrayAutostart),
#ifdef KOMAI_DBUS_SYS
    // IntegrationsDbusSection
    { QT_TR_NOOP("D-Bus"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsDbusApiAccess
    { QT_TR_NOOP("D-Bus access"),
      QT_TR_NOOP("Choose how much D-Bus access Komai exposes to local callers."),
      SM::Options, SM::TabIntegrations,
      integrationsDbusApiAccessValue,
      setIntegrationsDbusApiAccessValue,
      {}, {}, {},
      integrationsDbusApiAccessValues,
      nullptr,
      CORE_CONFIG_ID(IntegrationsDbusApiAccess, SettingKey::IntegrationsDbusApiAccess) },
#endif
    // IntegrationsMrsSection
    { QT_TR_NOOP("Matrix Rooms Search"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsMrsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enabled",
      QT_TR_NOOP("When enabled, adds <a href=\"https://github.com/etkecc/mrs\">Matrix Rooms Search</a> support to the <a href=\"komai://rooms-directory\">Explore Public Rooms</a> dialog."),
      SM::TabIntegrations, networkMrsEnabled, setNetworkMrsEnabled, nullptr, NetworkMrsEnabled, SettingKey::NetworkMrsEnabled),
    // IntegrationsMrsServerName
    SIMPLE_TEXT_CONFIG_ID_SETTING(
      "Server",
      QT_TR_NOOP("Any instance of <a href=\"https://github.com/etkecc/mrs\">Matrix Rooms Search</a> will do, but <a href=\"https://matrixrooms.info\">MatrixRooms.info</a> is a large popular instance run by <a href=\"https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=settings/integrations/mrs\">etke.cc</a>."),
      SM::TabIntegrations, networkMrsServerName, setNetworkMrsServerName, nullptr, NetworkMrsServerName, SettingKey::NetworkMrsServerName),

    // IntegrationsBrowserSection (control loaded via IntegrationsTab footerContent)
    { QT_TR_NOOP("Browser"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
