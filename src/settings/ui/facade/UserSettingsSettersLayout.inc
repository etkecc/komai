void
UserSettings::setUiFontSizePt(double size)
{
    constexpr double kMinFontSizePt = 6.0;
    constexpr double kMaxFontSizePt = 64.0;

    if (!std::isfinite(size) || size < kMinFontSizePt || size > kMaxFontSizePt)
        size = settings::core::definitions::kDefaultFontSizePt;

    if (size == uiFontSizePt_)
        return;
    uiFontSizePt_ = size;
    (void)coreStore_.set(settings::core::SettingId::UiFontSizePt, size);

    const static auto defaultFamily = QFont().defaultFamily();
    QFont f((uiFontFamily_.isEmpty() || uiFontFamily_ == QStringLiteral("default")) ? defaultFamily : uiFontFamily_);
    f.setPointSizeF(uiFontSizePt());
    QApplication::setFont(f);

    emit uiFontSizePtChanged(size);
    save();
}

void
UserSettings::setUiScaleFactor(double factor)
{
    if (settings::core::isScaleFactorInRange(factor))
        setSetting(uiScaleFactor_, factor, &UserSettings::uiScaleFactorChanged);
}

void
UserSettings::setUiFontFamily(QString family)
{
    if (family == uiFontFamily_)
        return;
    uiFontFamily_ = family;
    (void)coreStore_.set(settings::core::SettingId::UiFontFamily, family.toStdString());

    const static auto defaultFamily = QFont().defaultFamily();
    QFont f((family.isEmpty() || family == QStringLiteral("default")) ? defaultFamily : family);
    f.setPointSizeF(uiFontSizePt());
    QApplication::setFont(f);

    emit uiFontFamilyChanged(family);
    save();
}

void
UserSettings::setUiFontEmojiFamily(QString family)
{
    if (family == uiFontEmojiFamily_)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (uiFontEmojiFamily_.isEmpty()) {
        QFontDatabase::removeApplicationEmojiFontFamily(uiFontEmojiFamily_);
    }
#endif

    if (family.isEmpty()) {
        uiFontEmojiFamily_.clear();
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        QFontDatabase::addApplicationEmojiFontFamily(family);
#endif
        uiFontEmojiFamily_ = family;
    }
    (void)coreStore_.set(settings::core::SettingId::UiFontEmojiFamily, uiFontEmojiFamily_.toStdString());

    komai::logging::ui()->info("Emoji font: changed to \"{}\"",
                      uiFontEmojiFamily_.isEmpty() ? "system default" : uiFontEmojiFamily_.toStdString());

    emit uiFontEmojiFamilyChanged(family);
    save();
}

void
UserSettings::setUiLanguage(QString code)
{
    if (code == uiLanguage_)
        return;
    uiLanguage_ = code;
    (void)coreStore_.set(settings::core::SettingId::UiLanguage, code.toStdString());
    emit uiLanguageChanged(code);
    save();
}

void
UserSettings::setNetworkPresenceStatusPolicy(Presence s)
{
    if (networkPresenceStatusPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NetworkPresenceStatusPolicy,
                      static_cast<int>(s),
                      "network.presence.status_policy"))
        return;
    networkPresenceStatusPolicy_ = s;
    emit networkPresenceStatusPolicyChanged(s);
    save();
}
void
UserSettings::setTimelineMediaImageDisplay(ShowImage s)
{
    if (timelineMediaImageDisplay_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMediaImageDisplay,
                      static_cast<int>(s),
                      "timeline.media.image_display"))
        return;
    timelineMediaImageDisplay_ = s;
    emit timelineMediaImageDisplayChanged(s);
    save();
}
