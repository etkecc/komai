    // ── Calls Tab ───────────────────────────────────────────────────────────

    // CallsGeneralSection
    { QT_TR_NOOP("GENERAL"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableLegacyCalls
    { QT_TR_NOOP("Enable legacy calls"),
      QT_TR_NOOP("Show the call button in the message composer. This uses the old VoIP calling feature which may not work reliably. Element Call support is expected in a future release."),
      SM::Toggle, SM::TabCalls,
      []() -> QVariant { return I->enableLegacyCalls(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnableLegacyCalls(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseFallbackCallRelayServer
    { QT_TR_NOOP("Use fallback call relay server"),
      QT_TR_NOOP("Use turn.matrix.org as a fallback relay/STUN server when your homeserver does not provide one."),
      SM::Toggle, SM::TabCalls,
      []() -> QVariant { return I->useFallbackCallRelayServer(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseFallbackCallRelayServer(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // CallsDevicesSection
    { QT_TR_NOOP("DEVICES"), nullptr, SM::SectionTitle, SM::TabCalls,
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
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().names(false, I->microphone().toStdString()));
          I->setMicrophone(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(false, I->microphone().toStdString()));
      },
      nullptr },
    // Camera
    { QT_TR_NOOP("Camera"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(true, I->camera().toStdString()))
            .indexOf(I->camera());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().names(true, I->camera().toStdString()));
          I->setCamera(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(true, I->camera().toStdString()));
      },
      nullptr },
    // CameraResolution
    { QT_TR_NOOP("Camera resolution"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().resolutions(I->camera().toStdString()))
            .indexOf(I->cameraResolution());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().resolutions(I->camera().toStdString()));
          I->setCameraResolution(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().resolutions(I->camera().toStdString()));
      },
      nullptr },
    // CameraFrameRate
    { QT_TR_NOOP("Camera frame rate"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()))
            .indexOf(I->cameraFrameRate());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()));
          I->setCameraFrameRate(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()));
      },
      nullptr },
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
          if (v.userType() != QMetaType::Int) return false;
          int ringtone = v.toInt();
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
      nullptr },

