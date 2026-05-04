// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
integrationsDbusApiAccessValue()
{
    auto i = I;
    if (!i)
        return IntegrationsDbusAccessNone;
    return i->integrationsDbusApiAccess();
}

static bool
setIntegrationsDbusApiAccessValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyIndexedOption(
      v, IntegrationsDbusAccessNone, IntegrationsDbusAccessReadWrite, [i](int access) {
          i->setIntegrationsDbusApiAccess(access);
      });
}

// Source-English option list for cross-locale settings search. See
// SettingDescriptorCallbacksLookFeelTheme.inc header for the rationale.
static const char *const integrationsDbusApiAccessValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "None"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Read-only"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Read & write"),
  nullptr,
};

static QVariant
integrationsDbusApiAccessValues()
{
    return QStringList{
      QCoreApplication::translate("UserSettingsModel", "None"),
      QCoreApplication::translate("UserSettingsModel", "Read-only"),
      QCoreApplication::translate("UserSettingsModel", "Read & write"),
    };
}
