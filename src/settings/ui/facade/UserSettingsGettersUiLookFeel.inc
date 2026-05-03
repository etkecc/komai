QString
UserSettings::uiThemeSlug() const
{
    if (const auto value = coreStore_.valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
        value.has_value() && !value->empty())
        return QString::fromStdString(*value);
    return !uiThemeSlug_.isEmpty() ? uiThemeSlug_ : uiThemeSlugDefault_;
}

bool
UserSettings::uiAvatarsCircular() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::UiAvatarsCircular);
        value.has_value())
        return *value;
    return uiAvatarsCircular_;
}

bool
UserSettings::uiMotionAnimationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::UiMotionAnimationsEnabled);
        value.has_value())
        return *value;
    return uiMotionAnimationsEnabled_;
}

bool
UserSettings::uiInputTouchSwipeGesturesEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::UiInputTouchSwipeGesturesEnabled);
        value.has_value())
        return *value;
    return uiInputTouchSwipeGesturesEnabled_;
}

UserSettings::Density
UserSettings::uiLayoutDensity() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::UiLayoutDensity);
        value.has_value() && *value >= static_cast<int>(UserSettings::Density::Spacious) &&
        *value <= static_cast<int>(UserSettings::Density::Dense))
        return static_cast<UserSettings::Density>(*value);
    return uiLayoutDensity_;
}
