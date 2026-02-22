void
UserSettings::setFontSize(double size)
{
    constexpr double kDefaultFontSizePt = 13.0;
    constexpr double kMinFontSizePt    = 6.0;
    constexpr double kMaxFontSizePt    = 64.0;

    if (!std::isfinite(size) || size < kMinFontSizePt || size > kMaxFontSizePt)
        size = kDefaultFontSizePt;

    if (size == baseFontSize_)
        return;
    baseFontSize_ = size;

    const static auto defaultFamily = QFont().defaultFamily();
    QFont f((font_.isEmpty() || font_ == QStringLiteral("default")) ? defaultFamily : font_);
    f.setPointSizeF(fontSize());
    QApplication::setFont(f);

    emit fontSizeChanged(size);
    save();
}

void
UserSettings::setScaleFactor(double factor)
{
    if (!settings::core::isScaleFactorInRange(factor)) {
        if (factor >= 0.0)
            return;
        setSetting(scaleFactor_, factor, &UserSettings::scaleFactorChanged);
        return;
    }
    setSetting(scaleFactor_, factor, &UserSettings::scaleFactorChanged);
}

void
UserSettings::setFontFamily(QString family)
{
    if (family == font_)
        return;
    font_ = family;

    const static auto defaultFamily = QFont().defaultFamily();
    QFont f((family.isEmpty() || family == QStringLiteral("default")) ? defaultFamily : family);
    f.setPointSizeF(fontSize());
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

    nhlog::ui()->info("Emoji font: changed to \"{}\"",
                      emojiFont_.isEmpty() ? "system default" : emojiFont_.toStdString());

    emit emojiFontChanged(family);
    save();
}

void
UserSettings::setPresence(Presence s)
{
    setSetting(presence_, s, &UserSettings::presenceChanged);
}
void
UserSettings::setShowImage(ShowImage s)
{
    setSetting(showImage_, s, &UserSettings::showImageChanged);
}
