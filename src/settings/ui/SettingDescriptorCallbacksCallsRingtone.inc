// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
callRingtoneValue()
{
    auto i = I;
    if (!i)
        return 1;

    const auto v = i->callsAudioRingtone();
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
        if (filepath.isEmpty())
            return false;
        i->setCallsAudioRingtone(filepath);
        return true;
    } else if (ringtone == 0) {
        i->setCallsAudioRingtone(QStringLiteral("Mute"));
        return true;
    } else if (ringtone == 1) {
        const auto defaultRingtone =
          QString::fromLatin1(settings::core::definitions::kDefaultCallsAudioRingtone);
        i->setCallsAudioRingtone(defaultRingtone);
        return true;
    }

    return false;
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
    if (i && !list.contains(i->callsAudioRingtone()))
        list.push_back(i->callsAudioRingtone());

    return list;
}
