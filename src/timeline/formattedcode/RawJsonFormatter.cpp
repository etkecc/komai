// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/formattedcode/RawJsonFormatter.h"

#include "komai-rust-cxxbridge/ffi.h"
#include "timeline/litehtml/LitehtmlStylesheet.h"

namespace timeline::formattedcode {

QString
formatRawJsonForDialog(const QString &rawJson, const QPalette &palette)
{
    const auto background    = timeline::litehtml::codeBackgroundColor(palette);
    const auto backgroundStd = background.toStdString();
    const auto rawStd        = rawJson.toStdString();
    return QString::fromStdString(std::string(
      komai::rust::highlight_raw_json(::rust::Str(rawStd.data(), rawStd.size()),
                                      ::rust::Str(backgroundStd.data(), backgroundStd.size()))));
}

} // namespace timeline::formattedcode
