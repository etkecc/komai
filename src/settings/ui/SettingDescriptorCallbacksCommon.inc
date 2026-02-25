// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Helper: convert std::vector<std::string> to QStringList
static QStringList
vecToList(const std::vector<std::string> &vec)
{
    QStringList l;
    for (const auto &d : vec)
        l.push_back(QString::fromStdString(d));
    return l;
}

template<typename Setter>
bool
applyIndexedOption(const QVariant &value, int minInclusive, int maxInclusive, Setter setFromIndex)
{
    int index = 0;
    if (!readSettingValue(value, index))
        return false;
    if (index < minInclusive || index > maxInclusive)
        return false;

    setFromIndex(index);
    return true;
}

template<typename Setter>
bool
applyStringListSelection(const QVariant &value, const QStringList &choices, Setter setFromValue)
{
    int index = 0;
    if (!readSettingValue(value, index))
        return false;
    if (index < 0 || index >= choices.size())
        return false;

    setFromValue(choices.at(index));
    return true;
}
