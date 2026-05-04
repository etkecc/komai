// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

class UserSettings;

namespace settings::ui {
class SettingsSearchProxyModel;
}

class UserSettingsModel : public QAbstractListModel
{
    /**
     * UserSettingsModel adapts runtime setting metadata for QML presentation.
     *
     * It renders sections + setting rows as a list model, provides role-based values
     * for delegates, and forwards edits back to the singleton `UserSettings`.
     */
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)

public:
    enum SettingsTab
    {
        TabLookFeel,
        TabNavigation,
        TabTimeline,
        TabComposer,
        TabDesktop,
        TabCalls,
        TabNetwork,
        TabAccount,
        TabApplicationProfiles,
        TabIntegrations,
        TabAbout,
    };
    Q_ENUM(SettingsTab);

private:
public:
    enum Types
    {
        Toggle,
        ToggleWithDescription,
        ReadOnlyText,
        Options,
        OptionsWithDescription,
        SegmentedOptions,
        SearchableOptions,
        PresenceStatusMessageField,
        Integer,
        IntegerWithDescription,
        Double,
        SectionTitle,
        SectionBar,
        ManageIgnoredUsers,
        Link,
        ThemeSelector,
        TextInput,
        LogoutButton,
        ProfileButton,
        AccessTokenField,
        TimelinePreview,
        AvatarPreview,
        CommunityFilterRow,
        SpacesFilterSection,
    };
    Q_ENUM(Types);

    enum Roles
    {
        Name,
        Description,
        Icon,
        Value,
        Type,
        ValueLowerBound,
        ValueUpperBound,
        ValueStep,
        Values,
        Enabled,
        ThemeVariantValue,
        ThemeVariantValues,
        Tab,
        TagId,
        SyncedToMatrix,
    };

    UserSettingsModel(QObject *parent = nullptr);
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    Q_INVOKABLE QObject *modelForTab(int tab) const;

    QString searchQuery() const { return searchQuery_; }
    void setSearchQuery(const QString &query);

    // Number of settings rows in `tab` that match the current search query.
    // Section-title rows are not counted. When the query is empty, returns the
    // total number of non-section rows in `tab` (so callers can choose to hide
    // the badge themselves when query is empty).
    //
    // Adds +1 when the tab has any registered custom-QML keyword match
    // (Account profile fields, Integrations transcription/browser, Timeline
    // state events, etc.). The +1 is "indicative, not exact" — it tells the
    // sidebar badge that the tab carries a match even though that match
    // lives outside `settingsTable`.
    Q_INVOKABLE int matchCountForTab(int tab) const;

    // True when `tab` has any registered custom-QML keyword that matches the
    // current search query (any section). Drives the empty-state label inside
    // SettingsContent and the fully-custom AccountTab / ApplicationProfilesTab.
    Q_INVOKABLE bool tabHasCustomMatches(int tab) const;

    // True when the named section in `tab` (e.g. "profile", "thisDevice",
    // "transcription") has at least one keyword matching the current query.
    // Returns true when the query is empty so callers can use this directly
    // as the section's `visible` property: when nothing is searched, every
    // section stays visible. Drives section-level filtering inside the
    // custom-QML tabs.
    Q_INVOKABLE bool customSectionMatches(int tab, const QString &sectionId) const;

Q_SIGNALS:
    void searchQueryChanged();

private:
    void wireSettingConnections(UserSettings *settings);

    QString searchQuery_;
    mutable QHash<int, settings::ui::SettingsSearchProxyModel *> filteredModels_;
};
