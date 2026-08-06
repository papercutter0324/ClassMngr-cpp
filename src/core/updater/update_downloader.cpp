#include "update_downloader.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

#include <utility>

namespace
{
constexpr int DownloadTransferTimeoutMs = 30000;
constexpr qint64 HashChunkSize = 1024 * 1024;
constexpr qint64 OrphanRetentionDays = 30;
constexpr auto PartialStatus = "partial";
constexpr auto CompletedStatus = "completed";
constexpr auto MetadataSuffix = ".download.json";

QString defaultUpdateDownloadDirectory()
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

bool isSuccessfulHttpStatus(
    int status
    )
{
    return status >= 200 && status < 300;
}

QString artifactFileName(
    const QString& fileName
    )
{
    return QFileInfo(fileName).fileName();
}

bool isRecognizedUpdaterFile(
    const QString& name
    )
{
    static const QRegularExpression pattern(
        QStringLiteral(
            R"(^ClassMngr(?:-[A-Za-z0-9._-]+)?(?:\.exe|\.dmg|\.tar\.gz|\.bin)(?:\.part|\.download\.json)?$)"
            )
        );

    return pattern.match(name).hasMatch();
}

QDateTime fileTimestampUtc(
    const QFileInfo& info
    )
{
    return info.lastModified().toUTC();
}
}

UpdateDownloader::UpdateDownloader(
    QObject* parent,
    const QString& downloadDirectory,
    Clock clock
    )
    : QObject(parent)
    , m_downloadDirectory(
        downloadDirectory.trimmed().isEmpty()
            ? defaultUpdateDownloadDirectory()
            : QDir(downloadDirectory).absolutePath()
        )
    , m_clock(std::move(clock))
{
}

bool UpdateDownloader::isBusy() const
{
    return m_state == State::Preparing
        || m_state == State::Downloading
        || m_state == State::Verifying;
}

UpdateDownloader::State UpdateDownloader::state() const
{
    return m_state;
}

bool UpdateDownloader::hasResumableDownload(
    const UpdateArtifact& artifact
    ) const
{
    const QString finalPath =
        finalPathFor(artifact.fileName);
    const auto record =
        readRecordFile(
            finalPath + QString::fromLatin1(MetadataSuffix)
            );

    if (
        !record
        || !recordMatchesArtifact(*record, artifact)
        || record->status != QString::fromLatin1(PartialStatus)
        )
    {
        return false;
    }

    const QFileInfo partial(
        finalPath + QStringLiteral(".part")
        );

    return partial.isFile()
        && partial.size() > 0
        && partial.size() <= artifact.sizeBytes;
}

qint64 UpdateDownloader::resumableBytes(
    const UpdateArtifact& artifact
    ) const
{
    if (!hasResumableDownload(artifact))
    {
        return 0;
    }

    return QFileInfo(
        finalPathFor(artifact.fileName)
            + QStringLiteral(".part")
        ).size();
}

bool UpdateDownloader::hasCompletedDownload(
    const UpdateArtifact& artifact
    ) const
{
    const QString finalPath =
        finalPathFor(artifact.fileName);
    const auto record =
        readRecordFile(
            finalPath + QString::fromLatin1(MetadataSuffix)
            );

    if (
        !record
        || !recordMatchesArtifact(*record, artifact)
        || record->status != QString::fromLatin1(CompletedStatus)
        )
    {
        return false;
    }

    const QFileInfo completed(finalPath);
    return completed.isFile()
        && completed.size() == artifact.sizeBytes;
}

QString UpdateDownloader::downloadDirectory() const
{
    return m_downloadDirectory;
}

void UpdateDownloader::cleanupDownloads(
    CleanupMode mode,
    const std::optional<UpdateArtifact>& currentArtifact,
    const QString& downloadDirectory,
    const QDateTime& nowUtc
    )
{
    const QString directoryPath =
        downloadDirectory.trimmed().isEmpty()
            ? defaultUpdateDownloadDirectory()
            : QDir(downloadDirectory).absolutePath();
    QDir directory(directoryPath);

    if (!directory.exists())
    {
        return;
    }

    const QFileInfoList metadataFiles =
        directory.entryInfoList(
            {QStringLiteral("*") + QString::fromLatin1(MetadataSuffix)},
            QDir::Files | QDir::NoSymLinks
            );
    QSet<QString> ownedNames;

    for (const QFileInfo& metadataInfo : metadataFiles)
    {
        const auto record =
            readRecordFile(metadataInfo.absoluteFilePath());

        if (!record)
        {
            continue;
        }

        const QString name =
            artifactFileName(record->artifact.fileName);
        if (!isRecognizedUpdaterFile(name))
        {
            continue;
        }

        ownedNames.insert(name);
        ownedNames.insert(name + QStringLiteral(".part"));
        ownedNames.insert(name + QString::fromLatin1(MetadataSuffix));

        bool remove =
            mode == CleanupMode::RemoveAll;

        if (mode == CleanupMode::KeepOnlyArtifact)
        {
            remove =
                !currentArtifact
                || !recordMatchesArtifact(
                    *record,
                    *currentArtifact
                    );
        }

        if (remove)
        {
            QFile::remove(
                directory.filePath(name)
                );
            QFile::remove(
                directory.filePath(
                    name + QStringLiteral(".part")
                    )
                );
            QFile::remove(
                metadataInfo.absoluteFilePath()
                );
        }
    }

    const QFileInfoList files =
        directory.entryInfoList(
            QDir::Files | QDir::NoSymLinks
            );
    const QDateTime cutoff =
        nowUtc.toUTC().addDays(-OrphanRetentionDays);

    for (const QFileInfo& info : files)
    {
        if (
            ownedNames.contains(info.fileName())
            || !isRecognizedUpdaterFile(info.fileName())
            || fileTimestampUtc(info) >= cutoff
            )
        {
            continue;
        }

        QFile::remove(
            info.absoluteFilePath()
            );
    }
}

void UpdateDownloader::download(
    const UpdateArtifact& artifact
    )
{
    if (isBusy())
    {
        return;
    }

    if (
        !GitHubRelease::isAllowedDownloadUrl(artifact.url)
        || artifact.sizeBytes <= 0
        || artifact.sha256.trimmed().isEmpty()
        )
    {
        fail(
            tr("The update download metadata is invalid."),
            false
            );
        return;
    }

    ++m_generation;
    resetActiveIo();
    m_artifact =
        artifact;
    m_finalPath =
        finalPathFor(artifact.fileName);
    m_hash.reset();
    m_bytesWritten =
        0;
    m_restartedAfterRangeFailure =
        false;

    QDir directory(m_downloadDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
    {
        fail(
            tr("Unable to create update download directory."),
            false
            );
        return;
    }

    prepareDownload();
}

void UpdateDownloader::pause()
{
    if (!isBusy())
    {
        return;
    }

    ++m_generation;
    stopReply();

    if (m_outputFile.isOpen())
    {
        m_outputFile.flush();
        m_outputFile.close();
    }
    if (m_hashInput.isOpen())
    {
        m_hashInput.close();
    }

    if (QFileInfo::exists(partialPath()))
    {
        m_record.status =
            QString::fromLatin1(PartialStatus);
        m_record.updatedAtUtc =
            nowUtc();
        static_cast<void>(
            writeRecord()
            );
        m_bytesWritten =
            QFileInfo(partialPath()).size();
    }

    setState(State::Paused);
    emit downloadPaused(
        m_bytesWritten,
        m_artifact.sizeBytes
        );
}

void UpdateDownloader::cancel()
{
    pause();
}

void UpdateDownloader::discard()
{
    if (m_finalPath.isEmpty())
    {
        return;
    }

    ++m_generation;
    stopReply();
    resetActiveIo();
    removeCurrentFiles(true);
    m_bytesWritten =
        0;
    setState(State::Idle);
    emit downloadDiscarded();
}

void UpdateDownloader::prepareDownload()
{
    const auto existing =
        readRecord();

    if (
        existing
        && recordMatchesArtifact(*existing, m_artifact)
        )
    {
        m_record =
            *existing;

        if (
            m_record.status == QString::fromLatin1(CompletedStatus)
            && QFileInfo(m_finalPath).isFile()
            && QFileInfo(m_finalPath).size() == m_artifact.sizeBytes
            )
        {
            m_hashPurpose =
                HashPurpose::Completed;
            m_hashInput.setFileName(
                m_finalPath
                );
        }
        else if (
            m_record.status == QString::fromLatin1(PartialStatus)
            && QFileInfo(partialPath()).isFile()
            && QFileInfo(partialPath()).size() > 0
            && QFileInfo(partialPath()).size() <= m_artifact.sizeBytes
            )
        {
            m_hashPurpose =
                HashPurpose::Partial;
            m_hashInput.setFileName(
                partialPath()
                );
        }
    }

    if (m_hashPurpose == HashPurpose::None)
    {
        removeCurrentFiles(true);
        startNewDownload();
        return;
    }

    if (!m_hashInput.open(QIODevice::ReadOnly))
    {
        removeCurrentFiles(true);
        startNewDownload();
        return;
    }

    setState(State::Preparing);
    emit downloadPreparing(
        m_hashInput.size(),
        m_artifact.sizeBytes
        );
    m_hash.reset();
    m_bytesWritten =
        0;

    const quint64 generation =
        m_generation;
    QTimer::singleShot(
        0,
        this,
        [this, generation]()
        {
            hashNextChunk(generation);
        }
        );
}

void UpdateDownloader::hashNextChunk(
    quint64 generation
    )
{
    if (
        generation != m_generation
        || m_state != State::Preparing
        || !m_hashInput.isOpen()
        )
    {
        return;
    }

    const QByteArray chunk =
        m_hashInput.read(HashChunkSize);

    if (!chunk.isEmpty())
    {
        m_hash.addData(chunk);
        m_bytesWritten +=
            chunk.size();
        emit downloadProgress(
            m_bytesWritten,
            m_artifact.sizeBytes
            );
        QTimer::singleShot(
            0,
            this,
            [this, generation]()
            {
                hashNextChunk(generation);
            }
            );
        return;
    }

    if (m_hashInput.error() != QFileDevice::NoError)
    {
        m_hashInput.close();
        fail(
            tr("Unable to read the saved update download."),
            true
            );
        return;
    }

    m_hashInput.close();
    finishPreparingHash();
}

void UpdateDownloader::finishPreparingHash()
{
    if (m_hashPurpose == HashPurpose::Completed)
    {
        finishCompletedFileVerification();
        return;
    }

    if (m_bytesWritten == m_artifact.sizeBytes)
    {
        verifyAndFinalize();
        return;
    }

    m_outputFile.setFileName(
        partialPath()
        );
    if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        fail(
            tr("Unable to prepare the saved update download."),
            false
            );
        return;
    }

    startRequest(
        m_bytesWritten
        );
}

void UpdateDownloader::startNewDownload()
{
    m_hashPurpose =
        HashPurpose::None;
    m_hash.reset();
    m_bytesWritten =
        0;
    m_record =
        DownloadRecord();
    m_record.artifact =
        m_artifact;
    m_record.status =
        QString::fromLatin1(PartialStatus);
    m_record.createdAtUtc =
        nowUtc();
    m_record.updatedAtUtc =
        m_record.createdAtUtc;

    m_outputFile.setFileName(
        partialPath()
        );
    if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        fail(
            tr("Unable to prepare update download file."),
            false
            );
        return;
    }

    if (!writeRecord())
    {
        fail(
            tr("Unable to save update download metadata."),
            true
            );
        return;
    }

    startRequest(0);
}

void UpdateDownloader::startRequest(
    qint64 offset
    )
{
    m_requestOffset =
        offset;
    m_responseValidated =
        false;

    QNetworkRequest request(
        m_artifact.url
        );
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );
    request.setTransferTimeout(
        DownloadTransferTimeoutMs
        );

    if (offset > 0)
    {
        request.setRawHeader(
            QByteArrayLiteral("Range"),
            QByteArrayLiteral("bytes=")
                + QByteArray::number(offset)
                + QByteArrayLiteral("-")
            );

        const QString validator =
            !m_record.etag.isEmpty()
                ? m_record.etag
                : m_record.lastModified;
        if (!validator.isEmpty())
        {
            request.setRawHeader(
                QByteArrayLiteral("If-Range"),
                validator.toUtf8()
                );
        }
    }

    m_reply =
        m_network.get(request);

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
        [this](qint64 received, qint64)
        {
            emit downloadProgress(
                m_requestOffset + received,
                m_artifact.sizeBytes
                );
        }
        );
    connect(
        m_reply,
        &QNetworkReply::finished,
        this,
        &UpdateDownloader::handleFinished
        );

    setState(State::Downloading);
    emit downloadStarted();
    emit downloadProgress(
        offset,
        m_artifact.sizeBytes
        );
}

bool UpdateDownloader::validateResponse()
{
    if (m_responseValidated)
    {
        return true;
    }
    if (!m_reply)
    {
        return false;
    }

    const QVariant statusAttribute =
        m_reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
            );
    if (!statusAttribute.isValid())
    {
        return true;
    }

    const int status =
        statusAttribute.toInt();

    if (m_requestOffset > 0 && status == 200)
    {
        if (m_outputFile.isOpen())
        {
            m_outputFile.close();
        }
        if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            fail(
                tr("Unable to restart the update download."),
                false
                );
            return false;
        }

        m_hash.reset();
        m_bytesWritten =
            0;
        m_requestOffset =
            0;
    }
    else if (m_requestOffset > 0 && status == 206)
    {
        const QByteArray contentRange =
            m_reply->rawHeader(
                QByteArrayLiteral("Content-Range")
                );
        static const QRegularExpression pattern(
            QStringLiteral(R"(^bytes (\d+)-(\d+)/(\d+)$)")
            );
        const QRegularExpressionMatch match =
            pattern.match(
                QString::fromLatin1(contentRange).trimmed()
                );

        if (
            !match.hasMatch()
            || match.captured(1).toLongLong() != m_requestOffset
            || match.captured(3).toLongLong() != m_artifact.sizeBytes
            )
        {
            fail(
                tr("The update server returned an invalid resume range."),
                true
                );
            return false;
        }
    }
    else if (!isSuccessfulHttpStatus(status))
    {
        return false;
    }

    m_record.etag =
        QString::fromUtf8(
            m_reply->rawHeader(
                QByteArrayLiteral("ETag")
                )
            );
    m_record.lastModified =
        QString::fromUtf8(
            m_reply->rawHeader(
                QByteArrayLiteral("Last-Modified")
                )
            );
    m_record.updatedAtUtc =
        nowUtc();
    static_cast<void>(
        writeRecord()
        );
    m_responseValidated =
        true;
    return true;
}

void UpdateDownloader::handleReadyRead()
{
    if (
        !m_reply
        || !m_outputFile.isOpen()
        || !validateResponse()
        )
    {
        return;
    }

    const QByteArray data =
        m_reply->readAll();
    if (data.isEmpty())
    {
        return;
    }

    if (
        m_bytesWritten + data.size() > m_artifact.sizeBytes
        || m_outputFile.write(data) != data.size()
        )
    {
        fail(
            tr("Unable to write update download file."),
            true
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

    const int status =
        m_reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
            ).toInt();

    if (
        m_requestOffset > 0
        && status == 416
        && !m_restartedAfterRangeFailure
        )
    {
        stopReply();

        if (m_outputFile.isOpen())
        {
            m_outputFile.close();
        }

        if (
            QFileInfo(partialPath()).size()
            == m_artifact.sizeBytes
            )
        {
            m_bytesWritten =
                m_artifact.sizeBytes;
            verifyAndFinalize();
            return;
        }

        m_restartedAfterRangeFailure =
            true;
        removeCurrentFiles(false);
        startNewDownload();
        return;
    }

    handleReadyRead();
    if (!m_reply)
    {
        return;
    }

    const QNetworkReply::NetworkError networkError =
        m_reply->error();
    const QString networkErrorString =
        m_reply->errorString();
    const QVariant statusCode =
        m_reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
            );

    stopReply();

    if (m_outputFile.isOpen())
    {
        m_outputFile.flush();
        m_outputFile.close();
    }

    if (networkError != QNetworkReply::NoError)
    {
        fail(
            tr("Update download failed: %1")
                .arg(networkErrorString),
            false
            );
        return;
    }

    if (
        statusCode.isValid()
        && !isSuccessfulHttpStatus(statusCode.toInt())
        )
    {
        fail(
            tr("Update download failed: HTTP %1")
                .arg(statusCode.toInt()),
            false
            );
        return;
    }

    if (m_bytesWritten != m_artifact.sizeBytes)
    {
        fail(
            tr("Update download size did not match the GitHub release metadata."),
            false
            );
        return;
    }

    verifyAndFinalize();
}

void UpdateDownloader::verifyAndFinalize()
{
    setState(State::Verifying);
    emit downloadVerifying();

    const quint64 generation =
        m_generation;
    QTimer::singleShot(
        0,
        this,
        [this, generation]()
        {
            if (
                generation != m_generation
                || m_state != State::Verifying
                )
            {
                return;
            }

            const QString actualHash =
                QString::fromLatin1(
                    m_hash.result().toHex()
                    );
            if (
                actualHash.compare(
                    m_artifact.sha256,
                    Qt::CaseInsensitive
                    ) != 0
                )
            {
                fail(
                    tr("Update download checksum did not match the GitHub release metadata."),
                    true
                    );
                return;
            }

            QFile::remove(m_finalPath);
            if (!QFile::rename(partialPath(), m_finalPath))
            {
                fail(
                    tr("Unable to finalize update download."),
                    false
                    );
                return;
            }

            m_record.status =
                QString::fromLatin1(CompletedStatus);
            m_record.updatedAtUtc =
                nowUtc();
            if (!writeRecord())
            {
                QFile::remove(m_finalPath);
                fail(
                    tr("Unable to save verified update metadata."),
                    true
                    );
                return;
            }

            setState(State::Completed);
            emit downloadSucceeded(m_finalPath);
        }
        );
}

void UpdateDownloader::finishCompletedFileVerification()
{
    setState(State::Verifying);
    emit downloadVerifying();

    const QString actualHash =
        QString::fromLatin1(
            m_hash.result().toHex()
            );
    if (
        m_bytesWritten != m_artifact.sizeBytes
        || actualHash.compare(
            m_artifact.sha256,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        removeCurrentFiles(true);
        fail(
            tr("The saved update package could not be verified."),
            false
            );
        return;
    }

    setState(State::Completed);
    emit downloadSucceeded(m_finalPath);
}

void UpdateDownloader::fail(
    const QString& message,
    bool discardPartial
    )
{
    ++m_generation;
    stopReply();
    resetActiveIo();

    if (discardPartial)
    {
        removeCurrentFiles(false);
        m_bytesWritten =
            0;
    }
    else if (
        !m_finalPath.isEmpty()
        && QFileInfo::exists(partialPath())
        )
    {
        m_record.status =
            QString::fromLatin1(PartialStatus);
        m_record.updatedAtUtc =
            nowUtc();
        static_cast<void>(
            writeRecord()
            );
        m_bytesWritten =
            QFileInfo(partialPath()).size();
    }

    setState(State::Failed);
    emit downloadFailed(message);
}

void UpdateDownloader::stopReply()
{
    if (!m_reply)
    {
        return;
    }

    QNetworkReply* reply =
        std::exchange(
            m_reply,
            nullptr
            );
    disconnect(
        reply,
        nullptr,
        this,
        nullptr
        );
    reply->abort();
    reply->deleteLater();
}

void UpdateDownloader::resetActiveIo()
{
    if (m_outputFile.isOpen())
    {
        m_outputFile.close();
    }
    if (m_hashInput.isOpen())
    {
        m_hashInput.close();
    }

    m_outputFile.setFileName(QString());
    m_hashInput.setFileName(QString());
    m_hashPurpose =
        HashPurpose::None;
}

void UpdateDownloader::setState(
    State state
    )
{
    m_state =
        state;
}

QString UpdateDownloader::finalPathFor(
    const QString& fileName
    ) const
{
    return QDir(m_downloadDirectory).filePath(
        artifactFileName(fileName)
        );
}

QString UpdateDownloader::partialPath() const
{
    return m_finalPath
        + QStringLiteral(".part");
}

QString UpdateDownloader::metadataPath() const
{
    return m_finalPath
        + QString::fromLatin1(MetadataSuffix);
}

std::optional<UpdateDownloader::DownloadRecord>
UpdateDownloader::readRecord() const
{
    return readRecordFile(
        metadataPath()
        );
}

std::optional<UpdateDownloader::DownloadRecord>
UpdateDownloader::readRecordFile(
    const QString& path
    )
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }

    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(
            file.readAll(),
            &error
            );
    if (
        error.error != QJsonParseError::NoError
        || !document.isObject()
        )
    {
        return std::nullopt;
    }

    const QJsonObject object =
        document.object();
    DownloadRecord record;
    record.schemaVersion =
        object.value(QStringLiteral("schemaVersion")).toInt();
    record.artifact.platformKey =
        object.value(QStringLiteral("platform")).toString();
    record.artifact.url =
        QUrl(object.value(QStringLiteral("url")).toString());
    record.artifact.fileName =
        object.value(QStringLiteral("fileName")).toString();
    record.artifact.sha256 =
        object.value(QStringLiteral("sha256")).toString();
    record.artifact.sizeBytes =
        object.value(QStringLiteral("sizeBytes")).toInteger();
    record.etag =
        object.value(QStringLiteral("etag")).toString();
    record.lastModified =
        object.value(QStringLiteral("lastModified")).toString();
    record.status =
        object.value(QStringLiteral("status")).toString();
    record.createdAtUtc =
        QDateTime::fromString(
            object.value(QStringLiteral("createdAtUtc")).toString(),
            Qt::ISODateWithMs
            );
    record.updatedAtUtc =
        QDateTime::fromString(
            object.value(QStringLiteral("updatedAtUtc")).toString(),
            Qt::ISODateWithMs
            );

    if (
        record.schemaVersion != 1
        || artifactFileName(record.artifact.fileName).isEmpty()
        || record.artifact.sizeBytes <= 0
        || record.artifact.sha256.isEmpty()
        || (
            record.status != QString::fromLatin1(PartialStatus)
            && record.status != QString::fromLatin1(CompletedStatus)
            )
        )
    {
        return std::nullopt;
    }

    return record;
}

bool UpdateDownloader::writeRecord()
{
    if (m_finalPath.isEmpty())
    {
        return false;
    }

    QJsonObject object;
    object.insert(
        QStringLiteral("schemaVersion"),
        m_record.schemaVersion
        );
    object.insert(
        QStringLiteral("platform"),
        m_record.artifact.platformKey
        );
    object.insert(
        QStringLiteral("url"),
        m_record.artifact.url.toString()
        );
    object.insert(
        QStringLiteral("fileName"),
        artifactFileName(m_record.artifact.fileName)
        );
    object.insert(
        QStringLiteral("sha256"),
        m_record.artifact.sha256.toLower()
        );
    object.insert(
        QStringLiteral("sizeBytes"),
        m_record.artifact.sizeBytes
        );
    object.insert(
        QStringLiteral("etag"),
        m_record.etag
        );
    object.insert(
        QStringLiteral("lastModified"),
        m_record.lastModified
        );
    object.insert(
        QStringLiteral("status"),
        m_record.status
        );
    object.insert(
        QStringLiteral("createdAtUtc"),
        m_record.createdAtUtc.toUTC().toString(Qt::ISODateWithMs)
        );
    object.insert(
        QStringLiteral("updatedAtUtc"),
        m_record.updatedAtUtc.toUTC().toString(Qt::ISODateWithMs)
        );

    QSaveFile file(
        metadataPath()
        );
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    const QByteArray data =
        QJsonDocument(object).toJson(
            QJsonDocument::Compact
            );
    return file.write(data) == data.size()
        && file.commit();
}

bool UpdateDownloader::recordMatchesArtifact(
    const DownloadRecord& record,
    const UpdateArtifact& artifact
    )
{
    return artifactFileName(record.artifact.fileName)
            == artifactFileName(artifact.fileName)
        && record.artifact.platformKey == artifact.platformKey
        && record.artifact.url == artifact.url
        && record.artifact.sizeBytes == artifact.sizeBytes
        && record.artifact.sha256.compare(
            artifact.sha256,
            Qt::CaseInsensitive
            ) == 0;
}

QDateTime UpdateDownloader::nowUtc() const
{
    return m_clock
        ? m_clock().toUTC()
        : QDateTime::currentDateTimeUtc();
}

void UpdateDownloader::removeCurrentFiles(
    bool removeCompleted
    )
{
    if (m_finalPath.isEmpty())
    {
        return;
    }

    QFile::remove(partialPath());
    QFile::remove(metadataPath());
    if (removeCompleted)
    {
        QFile::remove(m_finalPath);
    }
}
