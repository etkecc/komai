// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/formattedcode/RawJsonFormatter.h"

#include "komai-rust-cxxbridge/ffi.h"

namespace timeline::formattedcode {

QString
formatRawJsonForDialog(const QString &rawJson, const QPalette &palette)
{
    const bool isDark = palette.color(QPalette::Base).lightness() < 128;
    const auto rawStd = rawJson.toStdString();
    return QString::fromStdString(std::string(
      komai::rust::highlight_raw_json(::rust::Str(rawStd.data(), rawStd.size()), isDark)));
}

} // namespace timeline::formattedcode
