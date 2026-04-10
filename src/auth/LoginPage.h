// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "matrix/backend/MatrixAuthService.h"
#include <QHostInfo>
#include <QObject>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QVariantList>

struct SSOProvider
{
    Q_GADGET
    Q_PROPERTY(QString avatarUrl READ avatarUrl CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString id READ id CONSTANT)

public:
    [[nodiscard]] QString avatarUrl() const { return avatarUrl_; }
    [[nodiscard]] QString name() const { return name_.toHtmlEscaped(); }
    [[nodiscard]] QString id() const { return id_; }

    QString avatarUrl_;
    QString name_;
    QString id_;
};

class LoginPage : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Login)

    Q_PROPERTY(QString mxid READ mxid WRITE setMxid NOTIFY matrixIdChanged)
    Q_PROPERTY(QString homeserver READ homeserver WRITE setHomeserver NOTIFY homeserverChanged)

    Q_PROPERTY(QString mxidError READ mxidError NOTIFY mxidErrorChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorOccurred)
    Q_PROPERTY(bool lookingUpHs READ lookingUpHs NOTIFY lookingUpHsChanged)
    Q_PROPERTY(bool homeserverValid READ homeserverValid NOTIFY lookingUpHsChanged)
    Q_PROPERTY(bool loggingIn READ loggingIn NOTIFY loggingInChanged)
    Q_PROPERTY(bool passwordSupported READ passwordSupported NOTIFY versionLookedUp)
    Q_PROPERTY(bool ssoSupported READ ssoSupported NOTIFY versionLookedUp)
    Q_PROPERTY(bool homeserverNeeded READ homeserverNeeded NOTIFY versionLookedUp)

    Q_PROPERTY(QVariantList identityProviders READ identityProviders NOTIFY versionLookedUp)

public:
    enum class LoginMethod
    {
        Password,
        SSO,
    };
    Q_ENUM(LoginMethod)

    LoginPage(QObject *parent = nullptr);

    Q_INVOKABLE QString initialDeviceName() const
    {
        return QString::fromStdString(initialDeviceName_());
    }

    Q_INVOKABLE QString deviceNameOS() const
    {
#if defined(Q_OS_MAC)
        return QStringLiteral("Komai-macOS");
#elif defined(Q_OS_LINUX)
        return QStringLiteral("Komai-Linux");
#elif defined(Q_OS_WIN)
        return QStringLiteral("Komai-Windows");
#elif defined(Q_OS_FREEBSD)
        return QStringLiteral("Komai-FreeBSD");
#elif defined(Q_OS_OPENBSD)
        return QStringLiteral("Komai-OpenBSD");
#else
        return QStringLiteral("Komai");
#endif
    }

    Q_INVOKABLE QString deviceNameHostname() const
    {
        return QStringLiteral("Komai-%1").arg(QHostInfo::localHostName());
    }

    Q_INVOKABLE QString deviceNameRandom() const
    {
        const auto &[adjectives, nouns] = randomWordLists_();
        auto *rng                       = QRandomGenerator::global();
        const auto adj                  = adjectives[rng->bounded(adjectives.size())];
        const auto noun                 = nouns[rng->bounded(nouns.size())];
        return QStringLiteral("Komai-%1-%2").arg(adj, noun);
    }

    // Returns the longest possible random device name (for stable UI sizing).
    Q_INVOKABLE QString deviceNameRandomMax() const
    {
        const auto &[adjectives, nouns] = randomWordLists_();
        int maxAdj                      = 0;
        int maxNoun                     = 0;
        QString longestAdj, longestNoun;
        for (const auto &a : adjectives) {
            if (a.size() > maxAdj) {
                maxAdj     = a.size();
                longestAdj = a;
            }
        }
        for (const auto &n : nouns) {
            if (n.size() > maxNoun) {
                maxNoun     = n.size();
                longestNoun = n;
            }
        }
        return QStringLiteral("Komai-%1-%2").arg(longestAdj, longestNoun);
    }

    bool lookingUpHs() const { return lookingUpHs_; }
    bool loggingIn() const { return loggingIn_; }
    bool passwordSupported() const { return passwordSupported_; }
    bool ssoSupported() const { return ssoSupported_; }
    bool homeserverNeeded() const { return homeserverNeeded_; }
    bool homeserverValid() const { return homeserverValid_; }
    QVariantList identityProviders() const { return identityProviders_; }

    QString homeserver() { return homeserver_; }
    QString mxid() { return mxid_; }

    QString error() { return error_; }
    QString mxidError() { return mxidError_; }

    void setHomeserver(const QString &hs);
    void setMxid(QString id)
    {
        if (id != mxid_) {
            mxid_ = id;
            emit matrixIdChanged();
            onMatrixIdEntered();
        }
    }

    static std::string initialDeviceName_()
    {
#if defined(Q_OS_MAC)
        return "Komai on macOS";
#elif defined(Q_OS_LINUX)
        return "Komai on Linux";
#elif defined(Q_OS_WIN)
        return "Komai on Windows";
#elif defined(Q_OS_FREEBSD)
        return "Komai on FreeBSD";
#elif defined(Q_OS_OPENBSD)
        return "Komai on OpenBSD";
#else
        return "Komai";
#endif
    }

signals:
    void loggingInChanged();
    void errorOccurred();

    //! Used to trigger the corresponding slot outside of the main thread.
    void versionErrorCb(const QString &err);
    void versionOkCb(bool passwordSupported,
                     bool ssoSupported,
                     bool oauthSupported,
                     QVariantList identityProviders);

    void loginOk(const komai::MatrixLoginResult &res);

    void onServerAddressEntered();

    void matrixIdChanged();
    void homeserverChanged();

    void mxidErrorChanged();
    void lookingUpHsChanged();
    void versionLookedUp();
    void versionLookupFinished();

public slots:
    // Displays errors produced during the login.
    void showError(const QString &msg);

    // Callback for the login button.
    void onLoginButtonClicked(LoginMethod loginMethod,
                              const QString &userid,
                              const QString &password,
                              const QString &deviceName);

    // Callback for errors produced during server probing
    void versionError(const QString &error_message);
    // Callback for successful server probing
    void versionOk(bool passwordSupported,
                   bool ssoSupported,
                   bool oauthSupported,
                   QVariantList identityProviders);

private:
    static const std::pair<QStringList, QStringList> &randomWordLists_()
    {
        static const std::pair<QStringList, QStringList> lists = {
          {
            QStringLiteral("swift"),  QStringLiteral("calm"),   QStringLiteral("bright"),
            QStringLiteral("quiet"),  QStringLiteral("bold"),   QStringLiteral("keen"),
            QStringLiteral("warm"),   QStringLiteral("cool"),   QStringLiteral("wild"),
            QStringLiteral("brave"),  QStringLiteral("clever"), QStringLiteral("gentle"),
            QStringLiteral("noble"),  QStringLiteral("vivid"),  QStringLiteral("steady"),
            QStringLiteral("lucky"),  QStringLiteral("nimble"), QStringLiteral("witty"),
            QStringLiteral("merry"),  QStringLiteral("cosmic"), QStringLiteral("snowy"),
            QStringLiteral("golden"), QStringLiteral("silver"), QStringLiteral("rustic"),
            QStringLiteral("misty"),  QStringLiteral("starry"), QStringLiteral("sandy"),
            QStringLiteral("mossy"),  QStringLiteral("coral"),  QStringLiteral("velvet"),
            QStringLiteral("amber"),  QStringLiteral("ivory"),  QStringLiteral("cedar"),
            QStringLiteral("maple"),  QStringLiteral("polar"),  QStringLiteral("lunar"),
            QStringLiteral("solar"),  QStringLiteral("alpine"), QStringLiteral("rusty"),
            QStringLiteral("azure"),  QStringLiteral("jade"),   QStringLiteral("copper"),
            QStringLiteral("sage"),   QStringLiteral("plum"),   QStringLiteral("olive"),
            QStringLiteral("dusky"),  QStringLiteral("frosty"), QStringLiteral("lively"),
            QStringLiteral("crisp"),  QStringLiteral("rosy"),
          },
          {
            QStringLiteral("owl"),    QStringLiteral("river"),  QStringLiteral("summit"),
            QStringLiteral("harbor"), QStringLiteral("fox"),    QStringLiteral("grove"),
            QStringLiteral("peak"),   QStringLiteral("brook"),  QStringLiteral("cliff"),
            QStringLiteral("reef"),   QStringLiteral("meadow"), QStringLiteral("falcon"),
            QStringLiteral("otter"),  QStringLiteral("cedar"),  QStringLiteral("maple"),
            QStringLiteral("canyon"), QStringLiteral("ridge"),  QStringLiteral("dune"),
            QStringLiteral("cove"),   QStringLiteral("trail"),  QStringLiteral("pine"),
            QStringLiteral("wolf"),   QStringLiteral("heron"),  QStringLiteral("pebble"),
            QStringLiteral("orchid"), QStringLiteral("ember"),  QStringLiteral("breeze"),
            QStringLiteral("moss"),   QStringLiteral("coral"),  QStringLiteral("stone"),
            QStringLiteral("fern"),   QStringLiteral("lark"),   QStringLiteral("wren"),
            QStringLiteral("crane"),  QStringLiteral("lotus"),  QStringLiteral("spark"),
            QStringLiteral("flint"),  QStringLiteral("drift"),  QStringLiteral("vale"),
            QStringLiteral("marsh"),  QStringLiteral("cloud"),  QStringLiteral("bloom"),
            QStringLiteral("shell"),  QStringLiteral("leaf"),   QStringLiteral("bay"),
            QStringLiteral("coast"),  QStringLiteral("tide"),   QStringLiteral("gale"),
          },
        };
        return lists;
    }

    void beginStartupRestoreHandoff();
    void startLoginFlowDiscovery(const QString &serverNameOrUrl, const QString &expectedHomeserver);
    QVariantList buildIdentityProviders(
      const std::vector<komai::MatrixLoginIdentityProvider> &identityProviders) const;
    void checkHomeserverVersion();
    void onMatrixIdEntered();
    void clearErrors()
    {
        error_.clear();
        mxidError_.clear();
        emit errorOccurred();
        emit mxidErrorChanged();
    }

    QString inferredServerAddress_;

    QString mxid_;
    QString homeserver_;

    QString mxidError_;
    QString error_;

    QVariantList identityProviders_;

    bool passwordSupported_ = true;
    bool ssoSupported_      = false;
    bool oauthSupported_    = false;

    bool lookingUpHs_                 = false;
    bool loggingIn_                   = false;
    bool homeserverNeeded_            = false;
    bool homeserverValid_             = false;
    bool startupRestoreHandoffActive_ = false;
};
