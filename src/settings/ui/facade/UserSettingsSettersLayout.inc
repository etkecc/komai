void
UserSettings::setUiFontSizePt(double size)
{
    constexpr double kMinFontSizePt = 6.0;
    constexpr double kMaxFontSizePt = 64.0;

    if (!std::isfinite(size) || size < kMinFontSizePt || size > kMaxFontSizePt)
        size = settings::core::definitions::kDefaultFontSizePt;

    if (size == baseFontSize_)
        return;
    baseFontSize_ = size;
    (void)coreStore_.set(settings::core::SettingId::UiFontSizePt, size);

    const static auto defaultFamily = QFont().defaultFamily();
    QFont f((font_.isEmpty() || font_ == QStringLiteral("default")) ? defaultFamily : font_);
    f.setPointSizeF(uiFontSizePt());
    QApplication::setFont(f);

    emit uiFontSizePtChanged(size);
    save();
}

void
UserSettings::setUiScaleFactor(double factor)
{
    if (!settings::core::isScaleFactorInRange(factor)) {
        if (factor >= 0.0)
            return;
        setSetting(uiScaleFactor_, factor, &UserSettings::uiScaleFactorChanged);
        return;
    }
    setSetting(uiScaleFactor_, factor, &UserSettings::uiScaleFactorChanged);
}

void
UserSettings::setFontFamily(QString family)
{
    if (family == font_)
        return;
    font_ = family;
    (void)coreStore_.set(settings::core::SettingId::UiFontFamily, family.toStdString());

    const static auto defaultFamily = QFont().defaultFamily();
    QFont f((family.isEmpty() || family == QStringLiteral("default")) ? defaultFamily : family);
    f.setPointSizeF(uiFontSizePt());
    QApplication::setFont(f);

    emit fontChanged(family);
    save();
}

void
UserSettings::setEmojiFontFamily(QString family)
{
    if (family == emojiFont_)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (emojiFont_.isEmpty()) {
        QFontDatabase::removeApplicationEmojiFontFamily(emojiFont_);
    }
#endif

    if (family.isEmpty()) {
        emojiFont_.clear();
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        QFontDatabase::addApplicationEmojiFontFamily(family);
#endif
        emojiFont_ = family;
    }
    (void)coreStore_.set(settings::core::SettingId::UiFontEmojiFamily, emojiFont_.toStdString());

    nhlog::ui()->info("Emoji font: changed to \"{}\"",
                      emojiFont_.isEmpty() ? "system default" : emojiFont_.toStdString());

    emit emojiFontChanged(family);
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
