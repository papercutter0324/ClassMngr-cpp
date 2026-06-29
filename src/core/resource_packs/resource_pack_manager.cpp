#include "resource_pack_manager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QResource>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
constexpr int InstalledMetadataSchemaVersion = 1;

QString sha256ForFile(
    const QString& filePath
    )
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);

    while (!file.atEnd())
    {
        const QByteArray data =
            file.read(1024 * 1024);

        if (data.isEmpty() && file.error() != QFile::NoError)
        {
            return QString();
        }

        hash.addData(data);
    }

    return QString::fromLatin1(
        hash.result().toHex()
        );
}

QString packRoot(
    const QString& packId
    )
{
    return QStringLiteral(":/resource-packs/%1")
        .arg(packId);
}
}

ResourcePackManager& ResourcePackManager::instance()
{
    static ResourcePackManager manager;
    return manager;
}

ResourcePackManager::ResourcePackManager(
    QString storageDirectory
    )
    : m_definitions({
        {
            QStringLiteral("campuses"),
            QStringLiteral(":/assets/campuses"),
            Version(1, 0, 0)
        },
        {
            QStringLiteral("templates"),
            QStringLiteral(":/assets/templates"),
            Version(1, 0, 0)
        },
        {
            QStringLiteral("roster-designs"),
            QStringLiteral(":/assets/roster-designs"),
            Version(1, 0, 0)
        },
        {
            QStringLiteral("book-reports"),
            QStringLiteral(":/assets/files/book reports"),
            Version(1, 0, 0)
        },
        {
            QStringLiteral("essay"),
            QStringLiteral(":/assets/files/essay"),
            Version(1, 0, 1)
        },
        {
            QStringLiteral("essay-topics"),
            QStringLiteral(":/assets/files/essay_topics"),
            Version(1, 0, 0)
        },
        {
            QStringLiteral("evaluations"),
            QStringLiteral(":/assets/files/evaluations"),
            Version(1, 0, 0)
        },
        {
            QStringLiteral("guides"),
            QStringLiteral(":/assets/files/guides"),
            Version(1, 0, 0)
        },
        {
            QStringLiteral("lessons"),
            QStringLiteral(":/assets/files/lessons"),
            Version(1, 0, 1)
        },
        {
            QStringLiteral("sub-prep"),
            QStringLiteral(":/assets/files/sub prep"),
            Version(1, 0, 0)
        },
        {
            QStringLiteral("training"),
            QStringLiteral(":/assets/files/training"),
            Version(1, 0, 0)
        }
    })
{
    if (!storageDirectory.trimmed().isEmpty())
    {
        m_storageDirectory =
            QDir::cleanPath(storageDirectory);
        return;
    }

    QString baseDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
            );

    if (baseDirectory.isEmpty())
    {
        baseDirectory =
            QDir::homePath() + QStringLiteral("/.ClassMngr");
    }

    m_storageDirectory =
        QDir(baseDirectory).filePath(
            QStringLiteral("resource-packs")
            );
}

Status ResourcePackManager::initialize()
{
    if (m_initialized)
    {
        return {};
    }

    m_initialized =
        true;

    QDir directory(m_storageDirectory);

    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
    {
        return std::unexpected(
            QStringLiteral("Unable to create the resource-pack directory.")
            );
    }

    QStringList errors;

    for (const Definition& packDefinition : m_definitions)
    {
        if (const Status status = loadInstalledPack(packDefinition); !status)
        {
            errors.append(status.error());
        }
    }

    removeStalePackFiles();

    if (!errors.isEmpty())
    {
        return std::unexpected(
            errors.join(QLatin1Char('\n'))
            );
    }

    return {};
}

QStringList ResourcePackManager::knownPackIds() const
{
    QStringList ids;

    for (const Definition& packDefinition : m_definitions)
    {
        ids.append(packDefinition.id);
    }

    return ids;
}

QString ResourcePackManager::activeRoot(
    const QString& packId
    ) const
{
    return m_activeRoots.value(packId);
}

QString ResourcePackManager::embeddedRoot(
    const QString& packId
    ) const
{
    const Definition* packDefinition =
        definition(packId);

    return packDefinition
        ? packDefinition->embeddedRoot
        : QString();
}

Version ResourcePackManager::currentVersion(
    const QString& packId
    ) const
{
    const auto activeIt =
        m_activeVersions.constFind(packId);

    if (activeIt != m_activeVersions.constEnd())
    {
        return activeIt.value();
    }

    const Definition* packDefinition =
        definition(packId);

    return packDefinition
        ? packDefinition->embeddedVersion
        : Version();
}

QString ResourcePackManager::storageDirectory() const
{
    return m_storageDirectory;
}

Status ResourcePackManager::stagePack(
    const ResourcePackArtifact& artifact,
    const QString& downloadedFilePath
    )
{
    if (!definition(artifact.id))
    {
        return std::unexpected(
            QStringLiteral("Unknown resource pack '%1'.")
                .arg(artifact.id)
            );
    }

    if (!artifact.version.isValid() || artifact.version <= currentVersion(artifact.id))
    {
        return std::unexpected(
            QStringLiteral("Resource pack '%1' is not newer than the active or embedded version.")
                .arg(artifact.id)
            );
    }

    const QFileInfo downloadedFile(downloadedFilePath);

    if (
        !downloadedFile.isFile()
        || downloadedFile.size() != artifact.sizeBytes
        || sha256ForFile(downloadedFilePath).compare(
            artifact.sha256,
            Qt::CaseInsensitive
            ) != 0
        )
    {
        return std::unexpected(
            QStringLiteral("Downloaded resource pack '%1' failed its integrity check.")
                .arg(artifact.id)
            );
    }

    const QString validationRoot =
        QStringLiteral("/__classmngr_pack_validation__/%1")
            .arg(artifact.id);

    if (!QResource::registerResource(downloadedFilePath, validationRoot))
    {
        return std::unexpected(
            QStringLiteral("Downloaded resource pack '%1' is not a valid RCC file.")
                .arg(artifact.id)
            );
    }

    const QString expectedValidationPath =
        QStringLiteral(":%1/resource-packs/%2")
            .arg(validationRoot, artifact.id);

    const bool hasExpectedRoot =
        QDir(expectedValidationPath).exists();

    QResource::unregisterResource(
        downloadedFilePath,
        validationRoot
        );

    if (!hasExpectedRoot)
    {
        return std::unexpected(
            QStringLiteral("Resource pack '%1' does not contain /resource-packs/%1.")
                .arg(artifact.id)
            );
    }

    const QString finalFileName =
        QStringLiteral("%1-%2.rcc")
            .arg(
                artifact.id,
                artifact.version.toString()
                );

    const QString finalPath =
        QDir(m_storageDirectory).filePath(finalFileName);

    if (QFileInfo(downloadedFilePath).absoluteFilePath() != QFileInfo(finalPath).absoluteFilePath())
    {
        QFile::remove(finalPath);

        if (!QFile::rename(downloadedFilePath, finalPath))
        {
            return std::unexpected(
                QStringLiteral("Unable to finalize resource pack '%1'.")
                    .arg(artifact.id)
                );
        }
    }

    QJsonObject metadata;
    metadata.insert(
        QStringLiteral("schemaVersion"),
        InstalledMetadataSchemaVersion
        );
    metadata.insert(
        QStringLiteral("id"),
        artifact.id
        );
    metadata.insert(
        QStringLiteral("version"),
        artifact.version.toString()
        );
    metadata.insert(
        QStringLiteral("fileName"),
        finalFileName
        );
    metadata.insert(
        QStringLiteral("sha256"),
        artifact.sha256.toLower()
        );

    QSaveFile metadataFile(
        QDir(m_storageDirectory).filePath(
            artifact.id + QStringLiteral(".json")
            )
        );

    if (!metadataFile.open(QIODevice::WriteOnly))
    {
        QFile::remove(finalPath);
        return std::unexpected(
            QStringLiteral("Unable to write metadata for resource pack '%1'.")
                .arg(artifact.id)
            );
    }

    metadataFile.write(
        QJsonDocument(metadata).toJson(QJsonDocument::Indented)
        );

    if (!metadataFile.commit())
    {
        QFile::remove(finalPath);
        return std::unexpected(
            QStringLiteral("Unable to commit metadata for resource pack '%1'.")
                .arg(artifact.id)
            );
    }

    return {};
}

const ResourcePackManager::Definition* ResourcePackManager::definition(
    const QString& packId
    ) const
{
    for (const Definition& packDefinition : m_definitions)
    {
        if (packDefinition.id == packId)
        {
            return &packDefinition;
        }
    }

    return nullptr;
}

Status ResourcePackManager::loadInstalledPack(
    const Definition& packDefinition
    )
{
    const QString metadataPath =
        QDir(m_storageDirectory).filePath(
            packDefinition.id + QStringLiteral(".json")
            );

    QFile metadataFile(metadataPath);

    if (!metadataFile.exists())
    {
        return {};
    }

    if (!metadataFile.open(QIODevice::ReadOnly))
    {
        return std::unexpected(
            QStringLiteral("Unable to read metadata for resource pack '%1'; using embedded resources.")
                .arg(packDefinition.id)
            );
    }

    const QJsonDocument document =
        QJsonDocument::fromJson(metadataFile.readAll());

    if (!document.isObject())
    {
        return std::unexpected(
            QStringLiteral("Metadata for resource pack '%1' is invalid; using embedded resources.")
                .arg(packDefinition.id)
            );
    }

    const QJsonObject metadata =
        document.object();

    if (
        metadata.value(QStringLiteral("schemaVersion")).toInt(-1)
            != InstalledMetadataSchemaVersion
        || metadata.value(QStringLiteral("id")).toString()
            != packDefinition.id
        )
    {
        return std::unexpected(
            QStringLiteral("Metadata for resource pack '%1' does not match the installed pack; using embedded resources.")
                .arg(packDefinition.id)
            );
    }

    const auto version =
        Version::parse(
            metadata.value(QStringLiteral("version")).toString()
            );

    const QString fileName =
        metadata.value(QStringLiteral("fileName")).toString();

    const QString expectedHash =
        metadata.value(QStringLiteral("sha256")).toString().toLower();

    if (
        !version
        || *version <= packDefinition.embeddedVersion
        || fileName.isEmpty()
        || QFileInfo(fileName).fileName() != fileName
        || expectedHash.size() != 64
        )
    {
        return std::unexpected(
            QStringLiteral("Metadata for resource pack '%1' is invalid; using embedded resources.")
                .arg(packDefinition.id)
            );
    }

    const QString filePath =
        QDir(m_storageDirectory).filePath(fileName);

    if (
        !QFileInfo::exists(filePath)
        || sha256ForFile(filePath).compare(expectedHash, Qt::CaseInsensitive) != 0
        )
    {
        return std::unexpected(
            QStringLiteral("Resource pack '%1' failed its integrity check; using embedded resources.")
                .arg(packDefinition.id)
            );
    }

    if (!QResource::registerResource(filePath))
    {
        return std::unexpected(
            QStringLiteral("Unable to mount resource pack '%1'; using embedded resources.")
                .arg(packDefinition.id)
            );
    }

    const QString root =
        packRoot(packDefinition.id);

    if (!QDir(root).exists())
    {
        QResource::unregisterResource(filePath);
        return std::unexpected(
            QStringLiteral("Resource pack '%1' has the wrong resource root; using embedded resources.")
                .arg(packDefinition.id)
            );
    }

    m_activeRoots.insert(
        packDefinition.id,
        root
        );
    m_activeVersions.insert(
        packDefinition.id,
        *version
        );
    m_activeFiles.insert(
        packDefinition.id,
        QFileInfo(filePath).absoluteFilePath()
        );

    return {};
}

void ResourcePackManager::removeStalePackFiles() const
{
    QDir directory(m_storageDirectory);

    const QStringList files =
        directory.entryList(
            {QStringLiteral("*.rcc")},
            QDir::Files
            );

    const QList<QString> activeFiles =
        m_activeFiles.values();

    for (const QString& fileName : files)
    {
        const QString absolutePath =
            QFileInfo(directory.filePath(fileName)).absoluteFilePath();

        if (!activeFiles.contains(absolutePath))
        {
            QFile::remove(absolutePath);
        }
    }
}
