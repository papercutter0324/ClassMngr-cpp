#pragma once

#include "core/updater/github_release.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>

#include <functional>
#include <optional>

class QNetworkReply;

class UpdateDownloader : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        Idle,
        Preparing,
        Downloading,
        Paused,
        Verifying,
        Completed,
        Failed
    };
    Q_ENUM(State)

    enum class CleanupMode
    {
        OrphansOnly,
        KeepOnlyArtifact,
        RemoveAll
    };

    using Clock =
        std::function<QDateTime()>;

    explicit UpdateDownloader(
        QObject* parent = nullptr,
        const QString& downloadDirectory = QString(),
        Clock clock = {}
        );

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] State state() const;
    [[nodiscard]] bool hasResumableDownload(
        const UpdateArtifact& artifact
        ) const;
    [[nodiscard]] qint64 resumableBytes(
        const UpdateArtifact& artifact
        ) const;
    [[nodiscard]] bool hasCompletedDownload(
        const UpdateArtifact& artifact
        ) const;
    [[nodiscard]] QString downloadDirectory() const;

    static void cleanupDownloads(
        CleanupMode mode,
        const std::optional<UpdateArtifact>& currentArtifact = std::nullopt,
        const QString& downloadDirectory = QString(),
        const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()
        );

public slots:
    void download(
        const UpdateArtifact& artifact
        );
    void pause();
    void cancel();
    void discard();

signals:
    void downloadPreparing(
        qint64 bytesAvailable,
        qint64 bytesTotal
        );
    void downloadStarted();
    void downloadProgress(
        qint64 bytesReceived,
        qint64 bytesTotal
        );
    void downloadPaused(
        qint64 bytesReceived,
        qint64 bytesTotal
        );
    void downloadVerifying();
    void downloadSucceeded(
        const QString& filePath
        );
    void downloadFailed(
        const QString& message
        );
    void downloadDiscarded();

private:
    struct DownloadRecord
    {
        int schemaVersion = 1;
        UpdateArtifact artifact;
        QString etag;
        QString lastModified;
        QString status;
        QDateTime createdAtUtc;
        QDateTime updatedAtUtc;
    };

    enum class HashPurpose
    {
        None,
        Partial,
        Completed
    };

    void prepareDownload();
    void hashNextChunk(
        quint64 generation
        );
    void finishPreparingHash();
    void startNewDownload();
    void startRequest(
        qint64 offset
        );
    bool validateResponse();
    void handleReadyRead();
    void handleFinished();
    void verifyAndFinalize();
    void finishCompletedFileVerification();
    void fail(
        const QString& message,
        bool discardPartial
        );
    void stopReply();
    void resetActiveIo();
    void setState(
        State state
        );

    [[nodiscard]] QString finalPathFor(
        const QString& fileName
        ) const;
    [[nodiscard]] QString partialPath() const;
    [[nodiscard]] QString metadataPath() const;
    [[nodiscard]] std::optional<DownloadRecord> readRecord() const;
    [[nodiscard]] static std::optional<DownloadRecord> readRecordFile(
        const QString& path
        );
    [[nodiscard]] bool writeRecord();
    [[nodiscard]] static bool recordMatchesArtifact(
        const DownloadRecord& record,
        const UpdateArtifact& artifact
        );
    [[nodiscard]] QDateTime nowUtc() const;
    void removeCurrentFiles(
        bool removeCompleted
        );

private:
    QNetworkAccessManager m_network;
    QNetworkReply* m_reply = nullptr;
    QFile m_outputFile;
    QFile m_hashInput;
    QString m_downloadDirectory;
    QString m_finalPath;
    UpdateArtifact m_artifact;
    DownloadRecord m_record;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    Clock m_clock;
    State m_state = State::Idle;
    HashPurpose m_hashPurpose = HashPurpose::None;
    qint64 m_bytesWritten = 0;
    qint64 m_requestOffset = 0;
    quint64 m_generation = 0;
    bool m_responseValidated = false;
    bool m_restartedAfterRangeFailure = false;
};
