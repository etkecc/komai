// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace emoji {
Q_NAMESPACE

struct Emoji
{
    Q_GADGET
public:
    enum class Category
    {
        People,
        Nature,
        Food,
        Activity,
        Travel,
        Objects,
        Symbols,
        Flags,
        Search
    };
    Q_ENUM(Category)

    Q_PROPERTY(QString unicode READ unicode CONSTANT)
    Q_PROPERTY(QString shortName READ shortName CONSTANT)
    Q_PROPERTY(QString unicodeName READ unicodeName CONSTANT)
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(emoji::Emoji::Category category MEMBER category)

public:
    Emoji(QString unicode, QString shortName, QString unicodeName, Category cat, QString id = {})
      : unicode_(unicode)
      , shortName_(shortName)
      , unicodeName_(unicodeName)
      , id_(id)
      , category(cat)
    {
    }

    Emoji()
      : unicode_()
      , shortName_()
      , unicodeName_()
      , id_()
      , category(Category::Search)
    {
    }

    Emoji(const Emoji &) = default;
    Emoji(Emoji &&)      = default;

    Emoji &operator=(const Emoji &) = default;
    Emoji &operator=(Emoji &&)      = default;

    QString unicode() const { return unicode_; }
    QString shortName() const { return shortName_; }
    QString unicodeName() const { return unicodeName_; }
    QString id() const { return id_; }

private:
    QString unicode_;
    QString shortName_;
    QString unicodeName_;
    QString id_;

public:
    Category category;
};

QString
categoryToName(emoji::Emoji::Category cat);
} // namespace emoji
