#pragma once

#include "core/updater/github_release.h"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>

class QNetworkReply;

class UpdateDownloader : public QObject
{
    Q_OBJECT

public:
    explicit UpdateDownloader(
        QObject* parent = nullptr
        );

    [[nodiscard]] bool isBusy() const;

public slots:
    void download(
        const UpdateArtifact& artifact
        );
    void cancel();

signals:
    void downloadStarted();
    void downloadProgress(
        qint64 bytesReceived,
        qint64 bytesTotal
        );
    void downloadSucceeded(
        const QString& filePath
        );
    void downloadFailed(
        const QString& message
        );
    void downloadCancelled();

private:
    void handleReadyRead();
    void handleFinished();
    void fail(
        const QString& message
        );
    void reset();

    [[nodiscard]] QString finalPathFor(
        const QString& fileName
        ) const;

private:
    QNetworkAccessManager m_network;
    QNetworkReply* m_reply = nullptr;
    QFile m_outputFile;
    QString m_finalPath;
    UpdateArtifact m_artifact;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    qint64 m_bytesWritten = 0;
    bool m_busy = false;
};
