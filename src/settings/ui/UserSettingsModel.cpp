// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSortFilterProxyModel>

#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/UserSettingsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"
#include "voip/CallDevices.h"

namespace settings::ui {

/**
 * Per-tab proxy that also applies the global search query from UserSettingsModel.
 *
 * Filtering rules:
 *   - When searchQuery is empty: rows are accepted iff their Tab role matches `tab_`.
 *   - When searchQuery is non-empty: rows are accepted iff their Tab role matches
 *     `tab_` AND (the row matches the query, OR — for SectionTitle rows — at least
 *     one non-section row in the same section + tab matches).
 *
 * "Matches" means a case-insensitive substring of the query is found in any of
 *   - the row's translated name / source-English name
 *   - the row's translated description / source-English description
 *   - the row's translated searchKeywords / source-English searchKeywords
 *   - any value in the row's option list, in either translated form
 *     (`m.getValues()`) or source-English form (`valuesEnglishFor(m.getValues)`)
 * The dual source/translated check is what makes search work in both English
 * (which is always present, regardless of UI language) and the active locale.
 */
class SettingsSearchProxyModel final : public QSortFilterProxyModel
{
public:
    SettingsSearchProxyModel(int tab, UserSettingsModel *parent)
      : QSortFilterProxyModel(parent)
      , tab_(tab)
      , settingsModel_(parent)
    {
        setSourceModel(parent);
    }

    void setQuery(const QString &q)
    {
        if (q == query_)
            return;
        query_ = q;
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    }

    int matchCount() const
    {
        const int total = settingsTableRowCount();
        int count       = 0;
        for (int row = 0; row < total; ++row) {
            const auto &m = settingsTable[row];
            if (m.tab != tab_)
                continue;
            if (m.type == UserSettingsModel::SectionTitle)
                continue;
            if (rowMatchesQuery(m))
                ++count;
        }
        return count;
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex & /*sourceParent*/) const override
    {
        if (sourceRow < 0 || sourceRow >= settingsTableRowCount())
            return false;

        const auto &m = settingsTable[sourceRow];
        if (m.tab != tab_)
            return false;

        if (query_.isEmpty())
            return true;

        if (m.type == UserSettingsModel::SectionTitle)
            return sectionHasMatch(sourceRow);

        return rowMatchesQuery(m);
    }

private:
    bool rowMatchesQuery(const SettingMeta &m) const
    {
        if (query_.isEmpty())
            return true;

        // Source-English form (works regardless of active locale).
        if (matchesSource(m.name) || matchesSource(m.description) ||
            matchesSource(m.searchKeywords))
            return true;

        // Translated form (current locale).
        if (matchesTranslated(m.name) || matchesTranslated(m.description) ||
            matchesTranslated(m.searchKeywords))
            return true;

        // Enum option labels (e.g. Density's "Compact" / "Spacious" / "Dense"):
        // match against both the translated list and the source-English array.
        if (m.getValues) {
            const QVariant translatedValues = m.getValues();
            for (const QString &v : translatedValues.toStringList()) {
                if (v.contains(query_, Qt::CaseInsensitive))
                    return true;
            }
            if (const char *const *english = valuesEnglishFor(m.getValues)) {
                for (const char *const *p = english; *p; ++p) {
                    if (matchesSource(*p))
                        return true;
                }
            }
        }

        return false;
    }

    bool matchesSource(const char *s) const
    {
        if (!s || !*s)
            return false;
        return QString::fromUtf8(s).contains(query_, Qt::CaseInsensitive);
    }

    bool matchesTranslated(const char *s) const
    {
        if (!s || !*s)
            return false;
        return settingsModel_->tr(s).contains(query_, Qt::CaseInsensitive);
    }

    // Walks forward from a SectionTitle row looking for any non-section row in
    // the same section (i.e. before the next SectionTitle in the same tab) that
    // matches the query. Stops on tab change to avoid leaking matches between
    // tabs in the source array.
    bool sectionHasMatch(int sectionRow) const
    {
        const int total = settingsTableRowCount();
        for (int r = sectionRow + 1; r < total; ++r) {
            const auto &row = settingsTable[r];
            if (row.tab != tab_)
                return false;
            if (row.type == UserSettingsModel::SectionTitle)
                return false;
            if (rowMatchesQuery(row))
                return true;
        }
        return false;
    }

    int tab_;
    QString query_;
    UserSettingsModel *settingsModel_;
};

} // namespace settings::ui

/**
 * UserSettingsModel is a UI adapter: it exposes settings metadata through roles,
 * groups by tab, and translates UI edits into `UserSettings` mutations.
 *
 * Storage/load semantics are implemented in `UserSettings` and `settings::*`
 * modules; this file intentionally contains list-model and delegate-facing
 * behavior only.
 */
QHash<int, QByteArray>
UserSettingsModel::roleNames() const
{
    static QHash<int, QByteArray> roles{
      {Name, "name"},
      {Description, "description"},
      {Icon, "icon"},
      {Value, "value"},
      {Type, "type"},
      {ValueLowerBound, "valueLowerBound"},
      {ValueUpperBound, "valueUpperBound"},
      {ValueStep, "valueStep"},
      {Values, "values"},
      {Enabled, "enabled"},
      {ThemeVariantValue, "themeVariantValue"},
      {ThemeVariantValues, "themeVariantValues"},
      {Tab, "tab"},
      {TagId, "tagId"},
      {SyncedToMatrix, "syncedToMatrix"},
    };

    return roles;
}

int
UserSettingsModel::rowCount(const QModelIndex &parent) const
{
    (void)parent;
    settings::ui::validateSettingsTable();
    return settings::ui::settingsTableRowCount();
}

QObject *
UserSettingsModel::modelForTab(int tab) const
{
    auto it = filteredModels_.find(tab);
    if (it != filteredModels_.end())
        return it.value();

    auto *proxyModel =
      new settings::ui::SettingsSearchProxyModel(tab, const_cast<UserSettingsModel *>(this));
    proxyModel->setQuery(searchQuery_);
    filteredModels_.insert(tab, proxyModel);

    return proxyModel;
}

void
UserSettingsModel::setSearchQuery(const QString &query)
{
    if (query == searchQuery_)
        return;
    searchQuery_ = query;
    for (auto *proxy : std::as_const(filteredModels_))
        proxy->setQuery(searchQuery_);
    emit searchQueryChanged();
}

int
UserSettingsModel::matchCountForTab(int tab) const
{
    auto *proxy = static_cast<settings::ui::SettingsSearchProxyModel *>(modelForTab(tab));
    return proxy->matchCount();
}

UserSettingsModel::UserSettingsModel(QObject *p)
  : QAbstractListModel(p)
{
    wireSettingConnections(UserSettings::instance().get());

    // tr() in data() picks up the new translation immediately, but views only
    // re-query on dataChanged. Without this, the currently-visible Settings
    // tab keeps showing the old language until the user switches tabs and
    // back, which forces a full re-render.
    //
    // QueuedConnection so the emit lands AFTER MainApplication's same-signal
    // slot has swapped translators — otherwise the view re-queries data()
    // while tr() still resolves against the old language.
    connect(
      UserSettings::instance().get(),
      &UserSettings::uiLanguageChanged,
      this,
      [this]() {
          const int rows = rowCount();
          if (rows > 0)
              emit dataChanged(index(0), index(rows - 1));
          // Translated label/description/keywords participate in search, so
          // re-evaluate filters for any active query.
          for (auto *proxy : std::as_const(filteredModels_))
              proxy->setQuery(searchQuery_);
      },
      Qt::QueuedConnection);

    connect(&CallDevices::instance(), &CallDevices::devicesChanged, this, [this]() {
        const auto emitCallDeviceRowUpdate = [this](settings::core::SettingId id) {
            const int row = settings::ui::rowForSettingId(id);
            if (row < 0)
                return;

            const QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx, {Value, Values});
        };

        emitCallDeviceRowUpdate(settings::core::SettingId::CallsDevicesMicrophone);
        emitCallDeviceRowUpdate(settings::core::SettingId::CallsDevicesCamera);
        emitCallDeviceRowUpdate(settings::core::SettingId::CallsDevicesCameraResolution);
        emitCallDeviceRowUpdate(settings::core::SettingId::CallsDevicesCameraFrameRate);
    });
}
