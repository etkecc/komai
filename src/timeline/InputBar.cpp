// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "InputBar.h"

#include <QBuffer>
#include <QClipboard>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputMethod>
#include <QMimeData>
#include <QStandardPaths>
#include <QTextBoundaryFinder>

#include "TimelineModel.h"
#include "TimelineViewManager.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

static constexpr size_t INPUT_HISTORY_SIZE = 10;

QUrl
MediaUpload::thumbnailDataUrl() const
{
    if (thumbnail_.isNull())
        return {};

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    thumbnail_.save(&buffer, "PNG");
    QString base64 = QString::fromUtf8(byteArray.toBase64());
    return QStringLiteral("data:image/png;base64,") + base64;
}

QString
InputBar::clipboardText() const
{
    return QGuiApplication::clipboard()->text();
}

bool
InputBar::tryPasteAttachment(bool fromMouse)
{
    const QMimeData *md = nullptr;

    if (fromMouse && QGuiApplication::clipboard()->supportsSelection()) {
        md = QGuiApplication::clipboard()->mimeData(QClipboard::Selection);
    } else {
        md = QGuiApplication::clipboard()->mimeData(QClipboard::Clipboard);
    }

    if (md)
        return insertMimeData(md);

    return false;
}

bool
InputBar::insertMimeData(const QMimeData *md)
{
    if (!md)
        return false;

    nhlog::ui()->debug("Got mime formats: {}",
                       md->formats().join(QStringLiteral(", ")).toStdString());
    nhlog::ui()->debug("Has image: {}", md->hasImage());
    const auto formats = md->formats().filter(QStringLiteral("/"));
    const auto image   = formats.filter(QStringLiteral("image/"), Qt::CaseInsensitive);
    const auto audio   = formats.filter(QStringLiteral("audio/"), Qt::CaseInsensitive);
    const auto video   = formats.filter(QStringLiteral("video/"), Qt::CaseInsensitive);

    if (md->hasImage()) {
        if (formats.contains(QStringLiteral("image/svg+xml"), Qt::CaseInsensitive)) {
            startUploadFromMimeData(*md, QStringLiteral("image/svg+xml"));
        } else if (formats.contains(QStringLiteral("image/png"), Qt::CaseInsensitive)) {
            startUploadFromMimeData(*md, QStringLiteral("image/png"));
        } else if (image.empty()) {
            QByteArray ba;
            QBuffer buffer(&ba);
            buffer.open(QIODevice::WriteOnly);
            qvariant_cast<QImage>(md->imageData()).save(&buffer, "PNG");
            QMimeData d;
            d.setData(QStringLiteral("image/png"), ba);
            startUploadFromMimeData(d, QStringLiteral("image/png"));
        } else {
            startUploadFromMimeData(*md, image.first());
        }
    } else if (!audio.empty()) {
        startUploadFromMimeData(*md, audio.first());
    } else if (!video.empty()) {
        startUploadFromMimeData(*md, video.first());
    } else if (md->hasUrls() &&
               // NOTE(Nico): Safari, when copying the url, sends a url list. Since we only paste
               // local files, skip remote ones.
               [&md] {
                   for (auto &&u : md->urls()) {
                       if (u.isLocalFile())
                           return true;
                   }
                   return false;
               }()) {
        // Generic file path for any platform.
        for (auto &&u : md->urls()) {
            if (u.isLocalFile()) {
                startUploadFromPath(u.toLocalFile());
            }
        }
    } else if (md->hasFormat(QStringLiteral("x-special/gnome-copied-files"))) {
        // Special case for X11 users. See "Notes for X11 Users" in md.
        // Source: http://doc.qt.io/qt-5/qclipboard.html

        // This MIME type returns a string with multiple lines separated by '\n'. The first
        // line is the command to perform with the clipboard (not useful to us). The
        // following lines are the file URIs.
        //
        // Source: the nautilus source code in file 'src/nautilus-clipboard.c' in function
        // nautilus_clipboard_get_uri_list_from_selection_data()
        // https://github.com/GNOME/nautilus/blob/master/src/nautilus-clipboard.c

        auto data = md->data(QStringLiteral("x-special/gnome-copied-files")).split('\n');
        if (data.size() < 2) {
            nhlog::ui()->warn("MIME format is malformed, cannot perform paste.");
            return false;
        }

        for (int i = 1; i < data.size(); ++i) {
            QUrl url{data[i]};
            if (url.isLocalFile()) {
                startUploadFromPath(url.toLocalFile());
            }
        }
    } else if (md->hasText()) {
        return false;
    } else {
        nhlog::ui()->debug("formats: {}", md->formats().join(QStringLiteral(", ")).toStdString());
        return false;
    }

    return true;
}

void
InputBar::addMention(QString mention, QString text)
{
    if (!mentions_.contains(mention)) {
        mentions_.push_back(mention);
        mentionTexts_.push_back(text);

        emit mentionsChanged();
    }
}

void
InputBar::removeMention(QString mention)
{
    if (auto idx = mentions_.indexOf(mention); idx != -1) {
        mentions_.removeAt(idx);
        mentionTexts_.removeAt(idx);
        emit mentionsChanged();
    }
}

void
InputBar::updateTextContentProperties(const QString &t, bool charDeleted)
{
    auto containsRoomMention = [](QStringView text) {
        // check for @room
        bool roomMention = false;
        if (text.size() > 4) {
            QTextBoundaryFinder finder(QTextBoundaryFinder::BoundaryType::Word, text);

            finder.toStart();
            do {
                auto start = finder.position();
                finder.toNextBoundary();
                auto end = finder.position();
                if (start > 0 && end - start >= 4 &&
                    text.mid(start, end - start) == QStringView(u"room") &&
                    text.at(start - 1) == QChar('@')) {
                    roomMention = true;
                    break;
                }
            } while (finder.position() < text.size());
        }
        return roomMention;
    };

    if (charDeleted) {
        for (qsizetype idx = 0; idx < mentions_.size();) {
            if (!t.contains(mentionTexts_.at(idx))) {
                removeMention(mentions_.at(idx));
            } else {
                idx++;
            }
        }
    }

    auto roomMention = containsRoomMention(t) && this->room->permissions()->canPingRoom();

    if (roomMention != this->containsAtRoom_) {
        this->containsAtRoom_ = roomMention;
        if (roomMention)
            addMention(QStringLiteral(u"@room"), QStringLiteral(u"@room"));
        else
            removeMention(QStringLiteral(u"@room"));
    }

    // check for invalid commands
    auto commandName = getCommandAndArgs(t).first;
    static const QSet<QString> validCommands{QStringLiteral("me"),
                                             QStringLiteral("react"),
                                             QStringLiteral("join"),
                                             QStringLiteral("knock"),
                                             QStringLiteral("part"),
                                             QStringLiteral("leave"),
                                             QStringLiteral("invite"),
                                             QStringLiteral("kick"),
                                             QStringLiteral("ban"),
                                             QStringLiteral("unban"),
                                             QStringLiteral("redact"),
                                             QStringLiteral("roomnick"),
                                             QStringLiteral("shrug"),
                                             QStringLiteral("fliptable"),
                                             QStringLiteral("unfliptable"),
                                             QStringLiteral("sovietflip"),
                                             QStringLiteral("clear-timeline"),
                                             QStringLiteral("reset-state"),
                                             QStringLiteral("rotate-megolm-session"),
                                             QStringLiteral("md"),
                                             QStringLiteral("cmark"),
                                             QStringLiteral("plain"),
                                             QStringLiteral("rainbow"),
                                             QStringLiteral("rainbowme"),
                                             QStringLiteral("notice"),
                                             QStringLiteral("rainbownotice"),
                                             QStringLiteral("confetti"),
                                             QStringLiteral("rainbowconfetti"),
                                             QStringLiteral("rainfall"),
                                             QStringLiteral("msgtype"),
                                             QStringLiteral("glitch"),
                                             QStringLiteral("gradualglitch"),
                                             QStringLiteral("goto"),
                                             QStringLiteral("converttodm"),
                                             QStringLiteral("converttoroom"),
                                             QStringLiteral("ignore"),
                                             QStringLiteral("unignore"),
                                             QStringLiteral("blockinvites"),
                                             QStringLiteral("allowinvites")};
    bool hasInvalidCommand    = !commandName.isNull() && !validCommands.contains(commandName);
    bool hasIncompleteCommand = hasInvalidCommand && '/' + commandName == t;

    bool signalsChanged{false};
    if (containsInvalidCommand_ != hasInvalidCommand) {
        containsInvalidCommand_ = hasInvalidCommand;
        signalsChanged          = true;
    }
    if (containsIncompleteCommand_ != hasIncompleteCommand) {
        containsIncompleteCommand_ = hasIncompleteCommand;
        signalsChanged             = true;
    }
    if (currentCommand_ != commandName) {
        currentCommand_ = commandName;
        signalsChanged  = true;
    }
    if (signalsChanged) {
        emit currentCommandChanged();
        emit containsInvalidCommandChanged();
        emit containsIncompleteCommandChanged();
    }
}

void
InputBar::setText(const QString &newText)
{
    if (history_.empty())
        history_.push_front(newText);
    else
        history_.front() = newText;
    history_index_ = 0;

    if (history_.size() == INPUT_HISTORY_SIZE)
        history_.pop_back();

    updateTextContentProperties(newText, true);
    emit textChanged(newText);
    emit draftTextChanged(newText);
}
void
InputBar::updateState(int selectionStart_,
                      int selectionEnd_,
                      int cursorPosition_,
                      const QString &text_)
{
    if (text_.isEmpty())
        stopTyping();
    else
        startTyping();

    auto oldText = text();
    if (text_ != oldText) {
        if (history_.empty())
            history_.push_front(text_);
        else
            history_.front() = text_;
        history_index_ = 0;

        updateTextContentProperties(text_, text_.size() < oldText.size());
        // disabled, as it moves the cursor to the end
        // emit textChanged(text_);
        emit draftTextChanged(text_);
    }

    selectionStart = selectionStart_;
    selectionEnd   = selectionEnd_;
    cursorPosition = cursorPosition_;
}

QString
InputBar::text() const
{
    if (history_index_ < history_.size())
        return history_.at(history_index_);

    return QString();
}

QString
InputBar::previousText()
{
    history_index_++;
    if (history_index_ >= INPUT_HISTORY_SIZE)
        history_index_ = INPUT_HISTORY_SIZE;
    else if (text().isEmpty())
        history_index_--;

    updateTextContentProperties(text());
    return text();
}

QString
InputBar::nextText()
{
    history_index_--;
    if (history_index_ >= INPUT_HISTORY_SIZE)
        history_index_ = 0;

    updateTextContentProperties(text());
    return text();
}

void
InputBar::send()
{
    QInputMethod *im = QGuiApplication::inputMethod();
    im->commit();

    bool hasUploads = !unconfirmedUploads.empty();
    bool hasText    = !text().trimmed().isEmpty();

    if (hasUploads) {
        if (hasText && allUploadsAreImages())
            caption_ = text().trimmed();

        acceptUploads();

        if (hasText) {
            history_.push_front(QLatin1String(""));
            setText(QLatin1String(""));
        }
        return;
    }

    if (!hasText)
        return;

    nhlog::ui()->debug("Send: {}", text().toStdString());

    auto wasEdit = !room->edit().isEmpty();

    auto [commandName, args] = getCommandAndArgs();
    updateTextContentProperties(text());
    if (containsIncompleteCommand_)
        return;
    if (commandName.isEmpty() || !command(commandName, args))
        message(text());

    if (!wasEdit) {
        history_.push_front(QLatin1String(""));
        setText(QLatin1String(""));
    }
}

void
InputBar::openFileSelection()
{
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QStringList fileNames =
      QFileDialog::getOpenFileNames(nullptr, tr("Select file(s)"), homeFolder, tr("All Files (*)"));

    if (fileNames.isEmpty())
        return;

    ChatPage::instance()->timelineManager()->focusMessageInput();
    for (const auto &fileName : fileNames)
        startUploadFromPath(fileName);
}

void
InputBar::startTyping()
{
    const bool typingSendEnabled =
      ChatPage::instance()->userSettings()->composerTypingSendEnabled();
    if (!typingSendEnabled) {
        typingRefresh_.stop();
        if (typingSent_)
            stopTyping();
        else
            typingTimeout_.stop();
        return;
    }

    if (!typingRefresh_.isActive()) {
        typingRefresh_.start();
        typingSent_ = true;
        http::client()->start_typing(
          room->roomId().toStdString(), 10'000, [](mtx::http::RequestErr err) {
              if (err) {
                  nhlog::net()->warn("failed to send typing notification: {}",
                                     err->matrix_error.error);
              }
          });
    }

    typingTimeout_.start();
}
void
InputBar::stopTyping()
{
    typingRefresh_.stop();
    typingTimeout_.stop();

    if (!typingSent_)
        return;

    typingSent_ = false;
    http::client()->stop_typing(room->roomId().toStdString(), [](mtx::http::RequestErr err) {
        if (err) {
            nhlog::net()->warn("failed to stop typing notifications: {}", err->matrix_error.error);
        }
    });
}

void
InputBar::reaction(const QString &reactedEvent, const QString &reactionKey)
{
    auto reactions = room->reactions(reactedEvent.toStdString());

    QString selfReactedEvent;
    for (const auto &reaction : reactions) {
        if (reactionKey == reaction.key_) {
            selfReactedEvent = reaction.selfReactedEvent_;
            break;
        }
    }

    if (selfReactedEvent.startsWith(QLatin1String("m")))
        return;

    // If selfReactedEvent is empty, that means we haven't previously reacted
    if (selfReactedEvent.isEmpty()) {
        mtx::events::msg::Reaction reaction;
        mtx::common::Relation rel;
        rel.rel_type = mtx::common::RelationType::Annotation;
        rel.event_id = reactedEvent.toStdString();
        rel.key      = reactionKey.toStdString();
        reaction.relations.relations.push_back(rel);

        room->sendMessageEvent(reaction, mtx::events::EventType::Reaction);
        // Otherwise, we have previously reacted and the reaction should be redacted
    } else {
        room->redactEvent(selfReactedEvent);
    }
}

#include "moc_InputBar.cpp"
