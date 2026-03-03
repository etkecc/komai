// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/format/TimelineImagePackFormatter.h"

#include <QCoreApplication>

#include "EventStore.h"
#include "Utils.h"

namespace {
QString
tr(const char *source)
{
    return QCoreApplication::translate("TimelineModel", source);
}
}

QString
timeline::format::formatImagePackEvent(
  const mtx::events::StateEvent<mtx::events::msc2545::ImagePack> &event,
  EventStore &eventStore,
  int imageAscent,
  const DisplayNameForUserFn &displayNameForUser)
{
    mtx::events::StateEvent<mtx::events::msc2545::ImagePack> const *prevEvent = nullptr;
    if (!event.unsigned_data.replaces_state.empty()) {
        auto tempPrevEvent = eventStore.get(event.unsigned_data.replaces_state, event.event_id);
        if (tempPrevEvent) {
            prevEvent =
              std::get_if<mtx::events::StateEvent<mtx::events::msc2545::ImagePack>>(tempPrevEvent);
        }
    }

    const auto &newImages = event.content.images;
    const auto oldImages  = prevEvent ? prevEvent->content.images : decltype(newImages){};

    auto calcChange = [imageAscent](
                        const std::map<std::string, mtx::events::msc2545::PackImage> &newI,
                        const std::map<std::string, mtx::events::msc2545::PackImage> &oldI) {
        QStringList added;
        for (const auto &[shortcode, img] : newI) {
            if (!oldI.count(shortcode))
                added.push_back(QStringLiteral("<img data-mx-emoticon height=%1 src=\"%2\"> (~%3)")
                                  .arg(imageAscent)
                                  .arg(QString::fromStdString(img.url)
                                         .replace("mxc://", "image://mxcImage/")
                                         .toHtmlEscaped(),
                                       QString::fromStdString(shortcode)));
        }
        return added;
    };

    auto added   = calcChange(newImages, oldImages);
    auto removed = calcChange(oldImages, newImages);

    auto sender = utils::replaceEmoji(displayNameForUser(QString::fromStdString(event.sender)));
    const auto packId = [&event]() -> QString {
        if (event.content.pack && !event.content.pack->display_name.empty()) {
            return event.content.pack->display_name.c_str();
        } else if (!event.state_key.empty()) {
            return event.state_key.c_str();
        }
        return tr("(empty)");
    }();

    QString msg;

    if (!removed.isEmpty()) {
        msg = tr("%1 removed the following images from the pack %2:<br>%3")
                .arg(sender, packId, removed.join(", "));
    }
    if (!added.isEmpty()) {
        if (!msg.isEmpty())
            msg += "<br>";
        msg += tr("%1 added the following images to the pack %2:<br>%3")
                 .arg(sender, packId, added.join(", "));
    }

    if (msg.isEmpty())
        return tr("%1 changed the sticker and emotes in this room.").arg(sender);
    else
        return msg;
}
