// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "encryption/Olm.h"

namespace olm {

mtx::crypto::OlmClient *
client()
{
    return nullptr;
}

DecryptionResult
decryptEvent(const MegolmSessionIndex &,
             const mtx::events::EncryptedEvent<mtx::events::msg::Encrypted> &,
             bool)
{
    return DecryptionResult{
      .error         = DecryptionErrorCode::MissingSession,
      .error_message = std::nullopt,
      .event         = std::nullopt,
    };
}

void
download_full_keybackup()
{
}

} // namespace olm
