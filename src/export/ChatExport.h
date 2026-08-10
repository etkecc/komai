// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <mutex>

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

/// Full-history chat export: walks a room's history backwards over
/// `/messages` on a worker thread, batch by batch, then renders and writes
/// a plain-text, HTML, or JSON Lines transcript. Surfaced in Room
/// settings -> Export.
///
/// Exports are per-room jobs: any number of rooms can export concurrently
/// (one job per room at a time), and every signal carries the room id so
/// each room's Export tab tracks only its own job.
class ChatExport final : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

public:
    enum class Format
    {
        PlainText,
        Html,
        JsonLines,
    };
    Q_ENUM(Format)

    static ChatExport *instance();

    // QML singleton entry point. Defined inline so qmltyperegistrar reliably
    // picks it up — the private constructor below ensures QML cannot fall
    // back to default-construction, which would silently create a second
    // instance and misroute signals.
    static ChatExport *create(QQmlEngine *, QJSEngine *) { return instance(); }

    Q_INVOKABLE bool isExporting(const QString &roomId) const;
    Q_INVOKABLE int fetchedCount(const QString &roomId) const;

    Q_INVOKABLE void startExport(const QString &roomId,
                                 const QString &roomName,
                                 const QUrl &file,
                                 Format format,
                                 bool includeMetadata);
    Q_INVOKABLE void cancel(const QString &roomId);

signals:
    void exportStarted(const QString &roomId);
    void progressChanged(const QString &roomId, int fetchedCount);
    void exportCompleted(const QString &roomId, int messageCount, int utdCount);
    void exportCancelled(const QString &roomId);
    void exportFailed(const QString &roomId, const QString &error);

private:
    struct ExportJob
    {
        std::atomic<int> fetched{0};
        std::atomic<bool> cancelRequested{false};
    };

    explicit ChatExport(QObject *parent = nullptr);

    std::shared_ptr<ExportJob> jobForRoom(const QString &roomId) const;

    mutable std::mutex jobsMutex_;
    QHash<QString, std::shared_ptr<ExportJob>> jobs_;
};
