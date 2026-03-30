// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <algorithm>
#include <cstdlib>
#include <string_view>

#include <QCoreApplication>
#include <QMessageBox>
#include <QTimer>

#include <nlohmann/json.hpp>

#include <mtx/secret_storage.hpp>
#include <mtxclient/crypto/client.hpp>

#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "cache/schema/CacheSchema.h"
#include "profile/ProfileSecrets.h"
#include "settings/SettingsStorage.h"
#include "settings/ui/facade/UserSettingsPage.h"

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
MatrixStore::loadSecretsFromStore(
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
            cache::activeLoggers().db->debug("Database ready");
        }
        return;
    }

    if (userSettings->usesFileSecretsProvider()) {
        for (auto &[name_, internal] : toLoad) {
            auto name  = secretName(name_, internal);
            auto value = userSettings->secret(name);
            if (value.isEmpty()) {
                cache::activeLoggers().db->info("Restored empty cache secret '{}'."
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

    auto name = secretName(name_, internal);
    settings::storage::readSecureValueAsync(
      name,
      this,
      [this, name, toLoad, name__ = name_, internal_ = internal, callback, databaseReadyOnFinished](
        const settings::storage::SecureBackendJobResult &result) mutable {
          cache::activeLoggers().db->debug("Finished reading '{}'", name.toStdString());
          const QString secret = result.value;
          if (result.failed()) {
              cache::activeLoggers().db->error("Restoring secret '{}' failed ({}): {}",
                                               name.toStdString(),
                                               result.errorCode,
                                               result.errorString.toStdString());

              fatalSecretError();
          }
          if (secret.isEmpty()) {
              cache::activeLoggers().db->debug(
                "Restored empty cache secret '{}'; scheduling cleanup.", name.toStdString());
              QTimer::singleShot(0, this, [name] {
                  auto userSettings = UserSettings::instance();
                  if (userSettings->usesFileSecretsProvider()) {
                      userSettings->removeSecret(name);
                      return;
                  }

                  const auto deleted = profile_secrets::deleteEmptyProfileSecretValueBlocking(name);
                  if (!deleted) {
                      cache::activeLoggers().db->warn(
                        "Failed to clean up stale empty cache secret '{}'.", name.toStdString());
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
    cache::activeLoggers().db->debug("Reading '{}'", name_);
}

std::optional<std::string>
MatrixStore::secret(std::string_view name_, bool internal)
{
    auto name = secretName(name_, internal);

    auto txn = ro_txn(storage());
    std::string_view value;
    if (!cache::sync_state::getSecretValue(txn, db->syncState, name.toStdString(), value))
        return std::nullopt;

    mtx::secret_storage::AesHmacSha2EncryptedData data = nlohmann::json::parse(value);

    auto decrypted = mtx::crypto::decrypt(data, mtx::crypto::to_binary_buf(pickle_secret_), name_);
    if (decrypted.empty())
        return std::nullopt;
    else
        return decrypted;
}

void
MatrixStore::storeSecret(std::string_view name_, const std::string &secret, bool internal)
{
    auto name = secretName(name_, internal);

    auto txn = beginTxn();

    auto encrypted =
      mtx::crypto::encrypt(secret, mtx::crypto::to_binary_buf(pickle_secret_), name_);

    cache::sync_state::putSecretValue(
      txn, db->syncState, name.toStdString(), nlohmann::json(encrypted).dump());
    txn.commit();
    emit secretChanged(std::string(name_));
}

void
MatrixStore::deleteSecret(std::string_view name_, bool internal)
{
    auto name = secretName(name_, internal);

    auto txn = beginTxn();
    cache::sync_state::removeSecretValue(txn, db->syncState, name.toStdString());
    txn.commit();
}

void
MatrixStore::storeSecretInStore(const std::string name_, const std::string secret)
{
    auto name         = secretName(name_, true);
    auto userSettings = UserSettings::instance();

    if (secret.empty()) {
        cache::activeLoggers().db->warn(
          "Refusing to store empty cache secret '{}'; deleting instead.", name_.c_str());
        deleteSecretFromStore(name_, true);
        return;
    }

    if (userSettings->usesFileSecretsProvider()) {
        userSettings->setSecret(name, QString::fromStdString(secret));
        // if we emit the signal directly it won't be received
        QTimer::singleShot(0, this, [this, name_] { emit secretChanged(name_); });
        cache::activeLoggers().db->info("Storing secret '{}' successful", name_);
        return;
    }

    settings::storage::writeSecureValueAsync(
      name,
      QString::fromStdString(secret),
      this,
      [name_, this](const settings::storage::SecureBackendJobResult &result) {
          if (!result.ok()) {
              cache::activeLoggers().db->warn(
                "Storing secret '{}' failed: {}", name_, result.errorString.toStdString());
              fatalSecretError();
          } else {
              // if we emit the signal directly, qtkeychain breaks and won't execute new
              // jobs. You can't start a job from the finish signal of a job.
              QTimer::singleShot(0, this, [this, name_] { emit secretChanged(name_); });
              cache::activeLoggers().db->info("Storing secret '{}' successful", name_);
          }
      });
}

void
MatrixStore::deleteSecretFromStore(const std::string name, bool internal)
{
    auto name_        = secretName(name, internal);
    auto userSettings = UserSettings::instance();

    if (userSettings->usesFileSecretsProvider()) {
        userSettings->removeSecret(name_);
        // if we emit the signal directly it won't be received
        QTimer::singleShot(0, this, [this, name] { emit secretChanged(name); });
        return;
    }

    settings::storage::deleteSecureValueAsync(
      name_, this, [this, name](const settings::storage::SecureBackendJobResult &result) {
          if (result.failed()) {
              cache::activeLoggers().db->warn(
                "Deleting secret '{}' failed: {}", name, result.errorString.toStdString());
          }
          emit secretChanged(name);
      });
}
