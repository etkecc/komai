// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <QUrl>

#include "Config.h"
#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "utils/Utils.h"

void
TimelineModel::setThread(const QString &id)
{
    if (id.isEmpty()) {
        resetThread();
        return;
    } else if (id != thread_) {
        thread_ = id;
        emit threadChanged(thread_);
    }
}

void
TimelineModel::resetThread()
{
    if (!thread_.isEmpty()) {
        thread_.clear();
        emit threadChanged(thread_);
    }
}

void
TimelineModel::setReply(const QString &newReply)
{
    if (reply_ != newReply) {
        reply_ = newReply;

        if (auto ev = events.get(reply_.toStdString(), "", false, true))
            setThread(QString::fromStdString(
              mtx::accessors::relations(*ev).thread().value_or(thread_.toStdString())));

        emit replyChanged(reply_);
    }
}

void
TimelineModel::populateEditMentions(const mtx::events::collections::TimelineEvents &event,
                                    QStringList &mentions,
                                    QStringList &mentionTexts) const
{
    auto mentionsList = mtx::accessors::mentions(event);
    if (!mentionsList)
        return;

    if (mentionsList->room) {
        mentions.append(QStringLiteral(u"@room"));
        mentionTexts.append(QStringLiteral(u"@room"));
    }

    for (const auto &user : mentionsList->user_ids) {
        auto userid = QString::fromStdString(user);
        mentions.append(userid);
        mentionTexts.append(QStringLiteral("[%1](https://matrix.to/#/%2)")
                              .arg(utils::escapeMentionMarkdown(
                                     // not using TimelineModel::displayName here,
                                     // because it would double html escape
                                     cache::displayName(room_id_, userid)),
                                   QString(QUrl::toPercentEncoding(userid))));
    }
}

QString
TimelineModel::normalizeFormattedEditText(QString editText,
                                          const QString &quotedFormattedBody) const
{
    if (quotedFormattedBody.isEmpty())
        return editText;

    auto matches = conf::strings::matrixToLink.globalMatch(quotedFormattedBody);
    std::map<QString, QString> reverseNameMapping;
    while (matches.hasNext()) {
        auto m                            = matches.next();
        reverseNameMapping[m.captured(2)] = m.captured(1);
    }

    for (const auto &[user, link] : reverseNameMapping) {
        // TODO(Nico): html unescape the user name
        editText.replace(user,
                         QStringLiteral("[%1](%2)").arg(utils::escapeMentionMarkdown(user), link));
    }

    return editText;
}

bool
TimelineModel::isEditableTextMessageType(mtx::events::MessageType msgType) const
{
    return msgType == mtx::events::MessageType::Text ||
           msgType == mtx::events::MessageType::Notice ||
           msgType == mtx::events::MessageType::Emote ||
           msgType == mtx::events::MessageType::ElementEffect ||
           msgType == mtx::events::MessageType::Unknown;
}

void
TimelineModel::applyEditedMessageText(const mtx::events::collections::TimelineEvents &event,
                                      mtx::events::MessageType msgType,
                                      const QString &editText)
{
    if (msgType == mtx::events::MessageType::Emote)
        input()->setText("/me " + editText);
    else if (msgType == mtx::events::MessageType::ElementEffect) {
        auto u = std::get_if<mtx::events::RoomEvent<mtx::events::msg::ElementEffect>>(&event);
        auto msgtypeString = u ? u->content.msgtype : "";
        if (msgtypeString == "io.element.effect.rainfall")
            input()->setText("/rainfall " + editText);
        else if (msgtypeString == "nic.custom.confetti")
            input()->setText("/confetti " + editText);
        else
            input()->setText("/msgtype " + QString::fromStdString(msgtypeString) + " " + editText);
    } else if (msgType == mtx::events::MessageType::Unknown) {
        auto u = std::get_if<mtx::events::RoomEvent<mtx::events::msg::Unknown>>(&event);
        input()->setText("/msgtype " + (u ? QString::fromStdString(u->content.msgtype) : "") + " " +
                         editText);
    } else
        input()->setText(editText);
}

void
TimelineModel::setEdit(const QString &newEdit)
{
    if (newEdit.isEmpty()) {
        resetEdit();
        return;
    }

    if (edit_.isEmpty()) {
        input()->storeForEdit();
    }

    if (edit_ != newEdit) {
        auto ev = events.get(newEdit.toStdString(), "");
        if (ev && mtx::accessors::sender(*ev) == utils::localUser().toStdString()) {
            auto e = *ev;
            setReply(QString::fromStdString(mtx::accessors::relations(e).reply_to().value_or("")));
            setThread(QString::fromStdString(mtx::accessors::relations(e).thread().value_or("")));

            QStringList mentions, mentionTexts;
            populateEditMentions(e, mentions, mentionTexts);

            auto msgType = mtx::accessors::msg_type(e);
            if (isEditableTextMessageType(msgType)) {
                auto relInfo = relatedInfo(newEdit);
                auto editText =
                  normalizeFormattedEditText(relInfo.quoted_body, relInfo.quoted_formatted_body);
                applyEditedMessageText(e, msgType, editText);
            } else {
                input()->setText(QLatin1String(""));
            }
            input()->replaceMentions(std::move(mentions), std::move(mentionTexts));

            edit_ = newEdit;
        } else {
            resetReply();

            input()->setText(QLatin1String(""));
            edit_ = QLatin1String("");
        }
        emit editChanged(edit_);
    }
}

void
TimelineModel::resetEdit()
{
    if (!edit_.isEmpty()) {
        edit_ = QLatin1String("");
        emit editChanged(edit_);
        input()->restoreAfterEdit();
        if (replyBeforeEdit.isEmpty())
            resetReply();
        else
            setReply(replyBeforeEdit);
        replyBeforeEdit.clear();
    }
}
