// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QStringList
callMicrophoneValuesList()
{
    auto i = I;
    if (!i)
        return {};
    return vecToList(CallDevices::instance().names(false, i->microphone().toStdString()));
}

static QVariant
callMicrophoneValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callMicrophoneValuesList().indexOf(i->microphone());
}

static bool
setCallMicrophoneValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    const auto list = callMicrophoneValuesList();
    if (idx < 0 || idx >= list.size())
        return false;

    i->setMicrophone(list.at(idx));
    return true;
}

static QVariant
callMicrophoneValues()
{
    return callMicrophoneValuesList();
}

static QStringList
callCameraValuesList()
{
    auto i = I;
    if (!i)
        return {};
    return vecToList(CallDevices::instance().names(true, i->camera().toStdString()));
}

static QVariant
callCameraValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraValuesList().indexOf(i->camera());
}

static bool
setCallCameraValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    const auto list = callCameraValuesList();
    if (idx < 0 || idx >= list.size())
        return false;

    i->setCamera(list.at(idx));
    return true;
}

static QVariant
callCameraValues()
{
    return callCameraValuesList();
}

static QStringList
callCameraResolutionValuesList()
{
    auto i = I;
    if (!i)
        return {};
    return vecToList(CallDevices::instance().resolutions(i->camera().toStdString()));
}

static QVariant
callCameraResolutionValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraResolutionValuesList().indexOf(i->cameraResolution());
}

static bool
setCallCameraResolutionValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    const auto list = callCameraResolutionValuesList();
    if (idx < 0 || idx >= list.size())
        return false;

    i->setCameraResolution(list.at(idx));
    return true;
}

static QVariant
callCameraResolutionValues()
{
    return callCameraResolutionValuesList();
}

static QStringList
callCameraFrameRateValuesList()
{
    auto i = I;
    if (!i)
        return {};

    return vecToList(CallDevices::instance().frameRates(i->camera().toStdString(),
                                                        i->cameraResolution().toStdString()));
}

static QVariant
callCameraFrameRateValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraFrameRateValuesList().indexOf(i->cameraFrameRate());
}

static bool
setCallCameraFrameRateValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    const auto list = callCameraFrameRateValuesList();
    if (idx < 0 || idx >= list.size())
        return false;

    i->setCameraFrameRate(list.at(idx));
    return true;
}

static QVariant
callCameraFrameRateValues()
{
    return callCameraFrameRateValuesList();
}

static QVariant
callRingtoneValue()
{
    auto i = I;
    if (!i)
        return 1;

    const auto v = i->ringtone();
    if (v == QStringView(u"Mute"))
        return 0;
    if (v == QString::fromLatin1(settings::core::definitions::kDefaultCallsAudioRingtone))
        return 1;
    if (v == QStringView(u"Other"))
        return 2;
    return 3;
}

static bool
setCallRingtoneValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int ringtone = 0;
    if (!readSettingValue(v, ringtone))
        return false;

    if (ringtone == 2) {
        const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        auto filepath            = QFileDialog::getOpenFileName(
          nullptr,
          QCoreApplication::translate("UserSettingsModel", "Select a file"),
          homeFolder,
          QCoreApplication::translate("UserSettingsModel", "All Files (*)"));
        if (!filepath.isEmpty()) {
            i->setRingtone(filepath);
            i->setRingtone(filepath);
        }
    } else if (ringtone == 0) {
        i->setRingtone(QStringLiteral("Mute"));
        i->setRingtone(QStringLiteral("Mute"));
    } else if (ringtone == 1) {
        const auto defaultRingtone =
          QString::fromLatin1(settings::core::definitions::kDefaultCallsAudioRingtone);
        i->setRingtone(defaultRingtone);
        i->setRingtone(defaultRingtone);
    }

    return true;
}

static QVariant
callRingtoneValues()
{
    QStringList list{
      QStringLiteral("Mute"),
      QString::fromLatin1(settings::core::definitions::kDefaultCallsAudioRingtone),
      QStringLiteral("Other"),
    };

    auto i = I;
    if (i && !list.contains(i->ringtone()))
        list.push_back(i->ringtone());

    return list;
}
