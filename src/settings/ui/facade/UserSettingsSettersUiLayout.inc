void
UserSettings::setUiLayoutContentMaxWidthPx(int s)
{
    const auto normalized = settings::core::definitions::normalizeUiLayoutContentMaxWidthPx(s);

    if (uiLayoutContentMaxWidthPx_ == normalized)
        return;
    if (!setCoreValue(settings::core::SettingId::UiLayoutContentMaxWidthPx,
                      normalized,
                      SettingKey::UiLayoutContentMaxWidthPx))
        return;
    uiLayoutContentMaxWidthPx_ = normalized;
    emit uiLayoutContentMaxWidthPxChanged(normalized);
    save();
}
