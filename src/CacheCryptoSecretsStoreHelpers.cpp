// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <algorithm>
#include <cstdlib>
#include <string_view>

#include <QCoreApplication>
#include <QMessageBox>
#include <QTimer>

#if __has_include(<keychain.h>)
#include <keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

#include <nlohmann/json.hpp>

#include <mtx/secret_storage.hpp>
#include <mtxclient/crypto/client.hpp>

#include "Logging.h"
#include "ProfileSecrets.h"
#include "UserSettingsPage.h"
#include "db/StorageApi.h"

static QString
secretName(std::string_view name, bool internal)
{
    return profile_secrets::cacheSecretStoreKey(
      UserSettings::instance()->profile(), name, internal);
}

static void
fatalSecretError()
{
    QMessageBox::critical(
      nullptr,
      QCoreApplication::translate("SecretStorage", "Failed to connect to secret storage"),
      QCoreApplication::translate(
        "SecretStorage",
        "Komai could not connect to the secure storage to save encryption secrets to. This can "
        "have multiple reasons. Check if your D-Bus service is running and you have configured a "
        "service like KWallet, Gnome Keyring, KeePassXC or the equivalent for your platform. If "
        "you are having trouble, feel free to open an issue here: "
        "https://github.com/etkecc/komai/issues"),
      QMessageBox::StandardButton::Close);

    QCoreApplication::exit(1);
    exit(1);
}

void
Cache::loadSecretsFromStore(
  std::vector<std::pair<std::string, bool>> toLoad,
  std::function<void(const std::string &name, bool internal, const std::string &value)> callback,
  bool databaseReadyOnFinished)
{
    auto userSettings = UserSettings::instance();

    if (toLoad.empty()) {
        this->databaseReady_ = true;

        // HACK(Nico): Some migrations would loop infinitely otherwise.
        // So we set the database to be ready, but not emit the signal, because that would start the
        // migrations again. :D
        if (databaseReadyOnFinished) {
            emit databaseReady();
            nhlog::db()->debug("Database ready");
        }
        return;
    }

    if (userSettings->runWithoutSecureSecretsService()) {
        for (auto &[name_, internal] : toLoad) {
            auto name  = secretName(name_, internal);
            auto value = userSettings->secret(name);
            if (value.isEmpty()) {
                nhlog::db()->info("Restored empty cache secret '{}'."
                                  " Removing in-memory secret value.",
                                  name.toStdString());
                userSettings->removeSecret(name);
            } else {
                callback(name_, internal, value.toStdString());
            }
        }
        // if we emit the DatabaseReady signal directly it won't be received
        QTimer::singleShot(0, this, [this, callback, databaseReadyOnFinished] {
            loadSecretsFromStore({}, callback, databaseReadyOnFinished);
        });
        return;
    }

    auto [name_, internal] = toLoad.front();

    auto job = new QKeychain::ReadPasswordJob(QCoreApplication::applicationName());
    job->setAutoDelete(true);
    job->setInsecureFallback(false);
    auto name = secretName(name_, internal);
    job->setKey(name);

    connect(job,
            &QKeychain::ReadPasswordJob::finished,
            this,
            [this,
             name,
             toLoad,
             job,
             name__    = name_,
             internal_ = internal,
             callback,
             databaseReadyOnFinished](QKeychain::Job *) mutable {
                nhlog::db()->debug("Finished reading '{}'", name.toStdString());
                const QString secret = job->textData();
                if (job->error() && job->error() != QKeychain::Error::EntryNotFound) {
                    nhlog::db()->error("Restoring secret '{}' failed ({}): {}",
                                       name.toStdString(),
                                       static_cast<int>(job->error()),
                                       job->errorString().toStdString());

                    fatalSecretError();
                }
                if (secret.isEmpty()) {
                    nhlog::db()->debug("Restored empty cache secret '{}'; scheduling cleanup.",
                                       name.toStdString());
                    QTimer::singleShot(0, this, [name] {
                        auto userSettings = UserSettings::instance();
                        if (userSettings->runWithoutSecureSecretsService()) {
                            userSettings->removeSecret(name);
                            return;
                        }

                        const auto deleted = profile_secrets::deleteEmptyProfileSecretValueBlocking(name);
                        if (!deleted) {
                            nhlog::db()->warn("Failed to clean up stale empty cache secret '{}'.",
                                              name.toStdString());
                        }
                    });
                } else {
                    callback(name__, internal_, secret.toStdString());
                }

                // load next secret
                toLoad.erase(toLoad.begin());

                // You can't start a job from the finish signal of the job.
                QTimer::singleShot(0, this, [this, toLoad, callback, databaseReadyOnFinished] {
                    loadSecretsFromStore(toLoad, callback, databaseReadyOnFinished);
                });
            });
    nhlog::db()->debug("Reading '{}'", name_);
    job->start();
}

std::optional<std::string>
Cache::secret(std::string_view name_, bool internal)
{
    auto name = secretName(name_, internal);

    auto txn = ro_txn(storage());
    std::string_view value;
    if (!db::getSyncStateSecretValue(txn, db->syncState, name.toStdString(), value))
        return std::nullopt;

    mtx::secret_storage::AesHmacSha2EncryptedData data = nlohmann::json::parse(value);

    auto decrypted = mtx::crypto::decrypt(data, mtx::crypto::to_binary_buf(pickle_secret_), name_);
    if (decrypted.empty())
        return std::nullopt;
    else
        return decrypted;
}

void
Cache::storeSecret(std::string_view name_, const std::string &secret, bool internal)
{
    auto name = secretName(name_, internal);

    auto txn = beginTxn();

    auto encrypted =
      mtx::crypto::encrypt(secret, mtx::crypto::to_binary_buf(pickle_secret_), name_);

    db::putSyncStateSecretValue(
      txn, db->syncState, name.toStdString(), nlohmann::json(encrypted).dump());
    txn.commit();
    emit secretChanged(std::string(name_));
}

void
Cache::deleteSecret(std::string_view name_, bool internal)
{
    auto name = secretName(name_, internal);

    auto txn = beginTxn();
    db::removeSyncStateSecretValue(txn, db->syncState, name.toStdString());
    txn.commit();
}

void
Cache::storeSecretInStore(const std::string name_, const std::string secret)
{
    auto name         = secretName(name_, true);
    auto userSettings = UserSettings::instance();

    if (secret.empty()) {
        nhlog::db()->warn("Refusing to store empty cache secret '{}'; deleting instead.",
                          name_.c_str());
        deleteSecretFromStore(name_, true);
        return;
    }

    if (userSettings->runWithoutSecureSecretsService()) {
        userSettings->setSecret(name, QString::fromStdString(secret));
        // if we emit the signal directly it won't be received
        QTimer::singleShot(0, this, [this, name_] { emit secretChanged(name_); });
        nhlog::db()->info("Storing secret '{}' successful", name_);
        return;
    }

    auto job = new QKeychain::WritePasswordJob(QCoreApplication::applicationName());
    job->setAutoDelete(true);
    job->setInsecureFallback(false);

    job->setKey(name);

    job->setTextData(QString::fromStdString(secret));

    QObject::connect(
      job,
      &QKeychain::WritePasswordJob::finished,
      this,
      [name_, this](QKeychain::Job *job) {
          if (job->error()) {
              nhlog::db()->warn(
                "Storing secret '{}' failed: {}", name_, job->errorString().toStdString());
              fatalSecretError();
          } else {
              // if we emit the signal directly, qtkeychain breaks and won't execute new
              // jobs. You can't start a job from the finish signal of a job.
              QTimer::singleShot(0, this, [this, name_] { emit secretChanged(name_); });
              nhlog::db()->info("Storing secret '{}' successful", name_);
          }
      },
      Qt::ConnectionType::DirectConnection);
    job->start();
}

void
Cache::deleteSecretFromStore(const std::string name, bool internal)
{
    auto name_        = secretName(name, internal);
    auto userSettings = UserSettings::instance();

    if (userSettings->runWithoutSecureSecretsService()) {
        userSettings->removeSecret(name_);
        // if we emit the signal directly it won't be received
        QTimer::singleShot(0, this, [this, name] { emit secretChanged(name); });
        return;
    }

    auto job = new QKeychain::DeletePasswordJob(QCoreApplication::applicationName());
    job->setAutoDelete(true);
    job->setInsecureFallback(false);

    job->setKey(name_);

    QObject::connect(
      job, &QKeychain::Job::finished, this, [this, name] { emit secretChanged(name); });
    job->start();
}
