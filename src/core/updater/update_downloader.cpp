#include "update_downloader.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

namespace
{
bool isSuccessfulHttpStatus(
    const QVariant& statusCode
    )
{
    if (!statusCode.isValid())
    {
        return true;
    }

    const int status =
        statusCode.toInt();

    return status >= 200 && status < 300;
}

QString updateDownloadDirectory()
{
    QString baseDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::TempLocation
            );

    if (baseDirectory.isEmpty())
    {
        baseDirectory =
            QDir::tempPath();
    }

    return QDir(baseDirectory).filePath(
        QStringLiteral("ClassMngr/updates")
        );
}
}

UpdateDownloader::UpdateDownloader(
    QObject* parent
    )
    : QObject(parent)
{
}

bool UpdateDownloader::isBusy() const
{
    return m_busy;
}

void UpdateDownloader::download(
    const UpdateArtifact& artifact
    )
{
    if (m_busy)
    {
        return;
    }

    if (!UpdateManifest::isAllowedDownloadUrl(artifact.url))
    {
        fail(
            tr("Update download URL must use HTTPS.")
            );
        return;
    }

    m_busy =
        true;
    m_artifact =
        artifact;
    m_hash.reset();
    m_bytesWritten =
        0;
    m_finalPath =
        finalPathFor(
            artifact.fileName
            );

    QDir directory(
        QFileInfo(m_finalPath).absolutePath()
        );

    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
    {
        fail(
            tr("Unable to create update download directory.")
            );
        return;
    }

    const QString partialPath =
        m_finalPath + QStringLiteral(".part");

    QFile::remove(partialPath);

    m_outputFile.setFileName(
        partialPath
        );

    if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        fail(
            tr("Unable to prepare update download file.")
            );
        return;
    }

    QNetworkRequest request(
        artifact.url
        );
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );

    m_reply =
        m_network.get(
            request
            );

    connect(
        m_reply,
        &QNetworkReply::readyRead,
        this,
        &UpdateDownloader::handleReadyRead
        );

    connect(
        m_reply,
        &QNetworkReply::downloadProgress,
        this,
        &UpdateDownloader::downloadProgress
        );

    connect(
        m_reply,
        &QNetworkReply::finished,
        this,
        &UpdateDownloader::handleFinished
        );

    emit downloadStarted();
}

void UpdateDownloader::handleReadyRead()
{
    if (!m_reply || !m_outputFile.isOpen())
    {
        return;
    }

    const QByteArray data =
        m_reply->readAll();

    if (data.isEmpty())
    {
        return;
    }

    if (m_outputFile.write(data) != data.size())
    {
        fail(
            tr("Unable to write update download file.")
            );
        return;
    }

    m_hash.addData(data);
    m_bytesWritten +=
        data.size();
}

void UpdateDownloader::handleFinished()
{
    if (!m_reply)
    {
        return;
    }

    handleReadyRead();

    const QNetworkReply::NetworkError networkError =
        m_reply->error();

    const QString networkErrorString =
        m_reply->errorString();

    const QVariant statusCode =
        m_reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
            );

    m_reply->deleteLater();
    m_reply =
        nullptr;

    if (m_outputFile.isOpen())
    {
        m_outputFile.close();
    }

    if (networkError != QNetworkReply::NoError)
    {
        fail(
            tr("Update download failed: %1")
                .arg(networkErrorString)
            );
        return;
    }

    if (!isSuccessfulHttpStatus(statusCode))
    {
        fail(
            tr("Update download failed: HTTP %1")
                .arg(statusCode.toInt())
            );
        return;
    }

    if (m_artifact.sizeBytes > 0 && m_bytesWritten != m_artifact.sizeBytes)
    {
        fail(
            tr("Update download size did not match the manifest.")
            );
        return;
    }

    const QString actualHash =
        QString::fromLatin1(
            m_hash.result().toHex()
            );

    if (actualHash.compare(m_artifact.sha256, Qt::CaseInsensitive) != 0)
    {
        fail(
            tr("Update download checksum did not match the manifest.")
            );
        return;
    }

    QFile::remove(m_finalPath);

    if (!QFile::rename(m_outputFile.fileName(), m_finalPath))
    {
        fail(
            tr("Unable to finalize update download.")
            );
        return;
    }

    const QString completedPath =
        m_finalPath;

    reset();

    emit downloadSucceeded(
        completedPath
        );
}

void UpdateDownloader::fail(
    const QString& message
    )
{
    const QString partialPath =
        m_outputFile.fileName();

    if (m_reply)
    {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply =
            nullptr;
    }

    if (m_outputFile.isOpen())
    {
        m_outputFile.close();
    }

    if (!partialPath.isEmpty())
    {
        QFile::remove(partialPath);
    }

    reset();

    emit downloadFailed(message);
}

void UpdateDownloader::reset()
{
    m_busy =
        false;
    m_outputFile.setFileName(QString());
    m_finalPath.clear();
    m_artifact =
        UpdateArtifact();
    m_hash.reset();
    m_bytesWritten =
        0;
}

QString UpdateDownloader::finalPathFor(
    const QString& fileName
    ) const
{
    QString safeFileName =
        QFileInfo(fileName).fileName();

    if (safeFileName.trimmed().isEmpty())
    {
        safeFileName =
            QStringLiteral("ClassMngr-update");
    }

    return QDir(updateDownloadDirectory()).filePath(
        safeFileName
        );
}
