// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

class QSortFilterProxyModel;
class UserSettings;

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

public:
    enum SettingsTab
    {
        TabLookFeel,
        TabSidebars,
        TabTimeline,
        TabComposer,
        TabNotifications,
        TabCalls,
        TabNetwork,
        TabPrivacy,
        TabEncryption,
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
        SearchableOptions,
        PresenceStatusMessageField,
        Integer,
        IntegerWithDescription,
        Double,
        SectionTitle,
        SectionBar,
        KeyStatus,
        SessionKeyImportExport,
        XSignKeysRequestDownload,
        ConfigureHiddenEvents,
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
        Good,
        Enabled,
        ThemeVariantValue,
        ThemeVariantValues,
        Tab,
        TagId,
    };

    UserSettingsModel(QObject *parent = nullptr);
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    Q_INVOKABLE QObject *modelForTab(int tab) const;
    Q_INVOKABLE void importSessionKeys();
    Q_INVOKABLE void exportSessionKeys();
    Q_INVOKABLE void requestCrossSigningSecrets();
    Q_INVOKABLE void downloadCrossSigningSecrets();

private:
    void wireSettingConnections(UserSettings *settings);

    mutable QHash<int, QSortFilterProxyModel *> filteredModels_;
};
