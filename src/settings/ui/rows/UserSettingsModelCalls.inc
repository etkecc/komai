    // ── Calls Tab ───────────────────────────────────────────────────────────

    // CallsGeneralSection
    { QT_TR_NOOP("General"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableLegacyCalls
    SIMPLE_BOOL_CONFIG_SETTING(
      "Enable legacy calls",
      QT_TR_NOOP("Show the call button in the message composer. This uses the old VoIP calling feature which may not work reliably. Element Call support is expected in a future release."),
      SM::TabCalls, enableLegacyCalls, setEnableLegacyCalls, nullptr, SettingKey::CallsLegacyEnabled),
    // UseFallbackCallRelayServer
    SIMPLE_BOOL_CONFIG_SETTING("Use fallback call relay server",
                        QT_TR_NOOP("Use turn.matrix.org as a fallback relay/STUN server when your homeserver does not provide one."),
                        SM::TabCalls, useFallbackCallRelayServer, setUseFallbackCallRelayServer, nullptr, SettingKey::CallsRelayUseFallbackServer),
    // CallsDevicesSection
    { QT_TR_NOOP("Devices"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Microphone
    { QT_TR_NOOP("Microphone"),
      QT_TR_NOOP("Select the microphone used for voice and video calls."),
      SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(false, I->microphone().toStdString()))
            .indexOf(I->microphone());
      },
    [](const QVariant &v) -> bool {
          int idx = 0;
          if (!readSettingValue(v, idx))
              return false;
          auto list = vecToList(CallDevices::instance().names(false, I->microphone().toStdString()));
          I->setMicrophone(list.at(idx)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(false, I->microphone().toStdString()));
      },
      nullptr,
      CORE_CONFIG(SettingKey::CallsDevicesMicrophone) },
    // Camera
    { QT_TR_NOOP("Camera"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(true, I->camera().toStdString()))
            .indexOf(I->camera());
      },
    [](const QVariant &v) -> bool {
          int idx = 0;
          if (!readSettingValue(v, idx))
              return false;
          auto list = vecToList(CallDevices::instance().names(true, I->camera().toStdString()));
          I->setCamera(list.at(idx)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(true, I->camera().toStdString()));
      },
      nullptr,
      CORE_CONFIG(SettingKey::CallsDevicesCamera) },
    // CameraResolution
    { QT_TR_NOOP("Camera resolution"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().resolutions(I->camera().toStdString()))
            .indexOf(I->cameraResolution());
      },
    [](const QVariant &v) -> bool {
          int idx = 0;
          if (!readSettingValue(v, idx))
              return false;
          auto list = vecToList(CallDevices::instance().resolutions(I->camera().toStdString()));
          I->setCameraResolution(list.at(idx)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().resolutions(I->camera().toStdString()));
      },
      nullptr,
      CORE_CONFIG(SettingKey::CallsDevicesCameraResolution) },
    // CameraFrameRate
    { QT_TR_NOOP("Camera frame rate"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()))
            .indexOf(I->cameraFrameRate());
      },
    [](const QVariant &v) -> bool {
          int idx = 0;
          if (!readSettingValue(v, idx))
              return false;
          auto list = vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()));
          I->setCameraFrameRate(list.at(idx)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()));
      },
      nullptr,
      CORE_CONFIG(SettingKey::CallsDevicesCameraFrameRate) },
    // Ringtone
    { QT_TR_NOOP("Ringtone"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          auto v = I->ringtone();
          if (v == QStringView(u"Mute")) return 0;
          if (v == QStringView(u"Default")) return 1;
          if (v == QStringView(u"Other")) return 2;
          return 3;
      },
    [](const QVariant &v) -> bool {
          int ringtone = 0;
          if (!readSettingValue(v, ringtone))
              return false;
          if (ringtone == 2) {
                QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
                auto filepath = QFileDialog::getOpenFileName(
                  nullptr,
                  QCoreApplication::translate("UserSettingsModel", "Select a file"),
                  homeFolder,
                  QCoreApplication::translate("UserSettingsModel", "All Files (*)"));
                if (!filepath.isEmpty()) { I->setRingtone(filepath); I->setRingtone(filepath); }
            } else if (ringtone == 0) {
                I->setRingtone(QStringLiteral("Mute")); I->setRingtone(QStringLiteral("Mute"));
            } else if (ringtone == 1) {
                I->setRingtone(QStringLiteral("Default")); I->setRingtone(QStringLiteral("Default"));
            }
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          QStringList l{QStringLiteral("Mute"), QStringLiteral("Default"), QStringLiteral("Other")};
          if (!l.contains(I->ringtone())) l.push_back(I->ringtone());
          return l;
      },
      nullptr,
      CORE_CONFIG(SettingKey::CallsAudioRingtone) },
