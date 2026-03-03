// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <fmt/ranges.h>

#include <ranges>

#include <mtx/secret_storage.hpp>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"

namespace {
void
unlock_secrets(const std::string &key,
               const std::map<std::string, mtx::secret_storage::AesHmacSha2EncryptedData> &secrets)
{
    http::client()->secret_storage_key(
      key,
      [secrets](mtx::secret_storage::AesHmacSha2KeyDescription keyDesc, mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->error("Failed to download secret storage key for {}: {}",
                                  fmt::join(std::views::keys(secrets), ", "),
                                  *err);
              return;
          }

          emit ChatPage::instance()->downloadedSecrets(keyDesc, secrets);
      });
}
} // namespace

namespace olm {

void
download_cross_signing_keys()
{
    using namespace mtx::secret_storage;
    http::client()->secret_storage_secret(
      secrets::megolm_backup_v1, [](Secret secret, mtx::http::RequestErr err) {
          std::optional<Secret> backup_key;
          if (!err)
              backup_key = secret;

          http::client()->secret_storage_secret(
            secrets::cross_signing_master, [backup_key](Secret secret, mtx::http::RequestErr err) {
                std::optional<Secret> master_key;
                if (!err)
                    master_key = secret;

                http::client()->secret_storage_secret(
                  secrets::cross_signing_self_signing,
                  [backup_key, master_key](Secret secret, mtx::http::RequestErr err) {
                      std::optional<Secret> self_signing_key;
                      if (!err)
                          self_signing_key = secret;

                      http::client()->secret_storage_secret(
                        secrets::cross_signing_user_signing,
                        [backup_key, self_signing_key, master_key](Secret secret,
                                                                   mtx::http::RequestErr err) {
                            std::optional<Secret> user_signing_key;
                            if (!err)
                                user_signing_key = secret;

                            std::map<std::string, std::map<std::string, AesHmacSha2EncryptedData>>
                              secrets;

                            if (backup_key && !backup_key->encrypted.empty())
                                secrets[backup_key->encrypted.begin()->first]
                                       [std::string(secrets::megolm_backup_v1)] =
                                         backup_key->encrypted.begin()->second;

                            if (master_key && !master_key->encrypted.empty())
                                secrets[master_key->encrypted.begin()->first]
                                       [std::string(secrets::cross_signing_master)] =
                                         master_key->encrypted.begin()->second;

                            if (self_signing_key && !self_signing_key->encrypted.empty())
                                secrets[self_signing_key->encrypted.begin()->first]
                                       [std::string(secrets::cross_signing_self_signing)] =
                                         self_signing_key->encrypted.begin()->second;

                            if (user_signing_key && !user_signing_key->encrypted.empty())
                                secrets[user_signing_key->encrypted.begin()->first]
                                       [std::string(secrets::cross_signing_user_signing)] =
                                         user_signing_key->encrypted.begin()->second;

                            for (const auto &[key, secret_] : secrets)
                                unlock_secrets(key, secret_);
                        });
                  });
            });
      });
}

} // namespace olm
