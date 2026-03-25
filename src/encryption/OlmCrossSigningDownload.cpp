// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include "logging/Logging.h"

namespace olm {

void
download_cross_signing_keys()
{
    nhlog::crypto()->warn(
      "Ignoring legacy cross-signing secret download; this flow is not migrated to the "
      "matrix-sdk backend yet");
}

} // namespace olm
