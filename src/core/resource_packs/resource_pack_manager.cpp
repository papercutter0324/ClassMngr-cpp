#include "resource_pack_manager.h"

#include "core/memory_usage_diagnostics.h"
#include "classmngr/engine/resource_pack_policy.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QResource>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace
{
constexpr int InstalledMetadataSchemaVersion = 1;

QString sha256ForFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        const QByteArray data = file.read(1024 * 1024);
        if (data.isEmpty() && file.error() != QFile::NoError)
        {
            return {};
        }
        hash.addData(data);
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString packRoot(const QString& packId)
{
    return QStringLiteral(":/resource-packs/%1").arg(packId);
}

QString defaultBaselineDirectory()
{
    const QString appDirectory = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MACOS
    const QString installedDirectory = QDir::cleanPath(
        appDirectory + QStringLiteral("/../Resources/resource-packs"));
#else
    const QString installedDirectory = QDir::cleanPath(
        appDirectory + QStringLiteral("/resources/resource-packs"));
#endif

    // Release packages keep resource packs next to the executable.  Prefer
    // that location even for a build configured with a development pack
    // directory: the latter is an absolute build-machine path and therefore
    // cannot exist on an end user's machine.
    if (QDir(installedDirectory).exists())
    {
        return installedDirectory;
    }

#ifdef CLASSMNGR_RESOURCE_PACK_DIR
    const QString configured = QStringLiteral(CLASSMNGR_RESOURCE_PACK_DIR);
    if (!configured.trimmed().isEmpty() && QDir(configured).exists())
    {
        return QDir::cleanPath(configured);
    }
#endif

    return installedDirectory;
}
}

ResourcePackLease::ResourcePackLease(
    ResourcePackManager* manager,
    QString packId,
    QString root
    )
    : m_manager(manager)
    , m_packId(std::move(packId))
    , m_root(std::move(root))
{
}

ResourcePackLease::~ResourcePackLease()
{
    reset();
}

ResourcePackLease::ResourcePackLease(ResourcePackLease&& other) noexcept
    : m_manager(std::exchange(other.m_manager, nullptr))
    , m_packId(std::move(other.m_packId))
    , m_root(std::move(other.m_root))
{
}

ResourcePackLease& ResourcePackLease::operator=(ResourcePackLease&& other) noexcept
{
    if (this != &other)
    {
        reset();
        m_manager = std::exchange(other.m_manager, nullptr);
        m_packId = std::move(other.m_packId);
        m_root = std::move(other.m_root);
    }
    return *this;
}

bool ResourcePackLease::isValid() const
{
    return m_manager != nullptr;
}

QString ResourcePackLease::packId() const
{
    return m_packId;
}

QString ResourcePackLease::root() const
{
    return m_root;
}

void ResourcePackLease::reset()
{
    if (m_manager)
    {
        m_manager->release(m_packId);
    }
    m_manager = nullptr;
    m_packId.clear();
    m_root.clear();
}

ResourcePackManager& ResourcePackManager::instance()
{
    static ResourcePackManager manager;
    return manager;
}

ResourcePackManager::ResourcePackManager(
    QString storageDirectory,
    QString baselineDirectory
    )
    : m_definitions({
        {QStringLiteral("campuses"), Version(1, 0, 0), true},
        {QStringLiteral("templates"), Version(1, 0, 0), true},
        {QStringLiteral("roster-designs"), Version(1, 0, 0), true},
        {QStringLiteral("documents"), Version(1, 0, 0), true},
        {QStringLiteral("files"), Version(1, 0, 0), false},
        {QStringLiteral("images"), Version(1, 0, 0), false},
        {QStringLiteral("splash"), Version(1, 0, 0), false}
    })
{
    if (!storageDirectory.trimmed().isEmpty())
    {
        m_storageDirectory = QDir::cleanPath(storageDirectory);
    }
    else
    {
        QString baseDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (baseDirectory.isEmpty())
        {
            baseDirectory = QDir::homePath() + QStringLiteral("/.ClassMngr");
        }
        m_storageDirectory = QDir(baseDirectory).filePath(QStringLiteral("resource-packs"));
    }

    m_baselineDirectory = baselineDirectory.trimmed().isEmpty()
        ? defaultBaselineDirectory()
        : QDir::cleanPath(baselineDirectory);
}

Status ResourcePackManager::initialize()
{
    if (m_initialized)
    {
        return {};
    }
    m_initialized = true;

    QDir directory(m_storageDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
    {
        return std::unexpected(QStringLiteral("Unable to create the resource-pack directory."));
    }

    QStringList errors;
    for (const Definition& packDefinition : m_definitions)
    {
        if (const Status status = discoverInstalledPack(packDefinition); !status)
        {
            errors.append(status.error());
        }
    }
    removeStalePackFiles();
    return errors.isEmpty()
        ? Status{}
        : Status(std::unexpected(errors.join(QLatin1Char('\n'))));
}

Result<ResourcePackLease> ResourcePackManager::acquire(const QString& packId)
{
    [[maybe_unused]] const Status initialized = initialize();
    const Definition* packDefinition = definition(packId);
    if (!packDefinition)
    {
        return std::unexpected(QStringLiteral("Unknown resource pack '%1'.").arg(packId));
    }
    if (const Status status = mount(*packDefinition); !status)
    {
        return std::unexpected(status.error());
    }

    MountedPack& mounted = m_mountedPacks[packId];
    ++mounted.leaseCount;
    return ResourcePackLease(this, packId, mounted.root);
}

bool ResourcePackManager::isMounted(const QString& packId) const
{
    return m_mountedPacks.contains(packId);
}

QString ResourcePackManager::activeRoot(const QString& packId) const
{
    const auto mounted = m_mountedPacks.constFind(packId);
    return mounted == m_mountedPacks.cend() ? QString() : mounted->root;
}

QStringList ResourcePackManager::knownPackIds() const
{
    QStringList ids;
    for (const Definition& packDefinition : m_definitions)
    {
        if (packDefinition.updateable)
        {
            ids.append(packDefinition.id);
        }
    }
    return ids;
}

Version ResourcePackManager::currentVersion(const QString& packId) const
{
    const auto installed = m_installedPacks.constFind(packId);
    if (installed != m_installedPacks.cend())
    {
        return installed->version;
    }
    const Definition* packDefinition = definition(packId);
    return packDefinition ? packDefinition->baselineVersion : Version();
}

QString ResourcePackManager::storageDirectory() const
{
    return m_storageDirectory;
}

QString ResourcePackManager::baselineDirectory() const
{
    return m_baselineDirectory;
}

Status ResourcePackManager::stagePack(
    const ResourcePackArtifact& artifact,
    const QString& downloadedFilePath
    )
{
    const Definition* packDefinition = definition(artifact.id);
    if (!packDefinition || !packDefinition->updateable)
    {
        return std::unexpected(QStringLiteral("Unknown resource pack '%1'.").arg(artifact.id));
    }
    if (!artifact.version.isValid() || artifact.version <= currentVersion(artifact.id))
    {
        return std::unexpected(QStringLiteral("Resource pack '%1' is not newer than the installed baseline.").arg(artifact.id));
    }

    const QFileInfo downloadedFile(downloadedFilePath);
    const auto policyVersion = classmngr::engine::SemanticVersion::parse(
        artifact.version.toString().toUtf8().toStdString()
        );
    const classmngr::engine::ResourcePackArtifact policyArtifact{
        artifact.id.toUtf8().toStdString(),
        policyVersion ? *policyVersion : classmngr::engine::SemanticVersion(),
        artifact.url.toString(QUrl::FullyEncoded).toUtf8().toStdString(),
        artifact.fileName.toUtf8().toStdString(),
        artifact.sha256.toUtf8().toStdString(),
        artifact.sizeBytes
    };
    if (!downloadedFile.isFile()
        || !policyVersion
        || !classmngr::engine::ResourcePackPolicy::acceptsDownload(
            policyArtifact,
            downloadedFile.size(),
            sha256ForFile(downloadedFilePath).toUtf8().toStdString()
            ))
    {
        return std::unexpected(QStringLiteral("Downloaded resource pack '%1' failed its integrity check.").arg(artifact.id));
    }

    const QString validationRoot = QStringLiteral("/__classmngr_pack_validation__/%1").arg(artifact.id);
    if (!QResource::registerResource(downloadedFilePath, validationRoot))
    {
        return std::unexpected(QStringLiteral("Downloaded resource pack '%1' is not a valid RCC file.").arg(artifact.id));
    }
    const bool hasExpectedRoot = QDir(QStringLiteral(":%1/resource-packs/%2").arg(validationRoot, artifact.id)).exists();
    QResource::unregisterResource(downloadedFilePath, validationRoot);
    if (!hasExpectedRoot)
    {
        return std::unexpected(QStringLiteral("Resource pack '%1' has the wrong root.").arg(artifact.id));
    }

    const QString finalFileName = QStringLiteral("%1-%2.rcc").arg(artifact.id, artifact.version.toString());
    const QString finalPath = QDir(m_storageDirectory).filePath(finalFileName);
    if (QFileInfo(downloadedFilePath).absoluteFilePath() != QFileInfo(finalPath).absoluteFilePath())
    {
        QFile::remove(finalPath);
        if (!QFile::rename(downloadedFilePath, finalPath))
        {
            return std::unexpected(QStringLiteral("Unable to finalize resource pack '%1'.").arg(artifact.id));
        }
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("schemaVersion"), InstalledMetadataSchemaVersion);
    metadata.insert(QStringLiteral("id"), artifact.id);
    metadata.insert(QStringLiteral("version"), artifact.version.toString());
    metadata.insert(QStringLiteral("fileName"), finalFileName);
    metadata.insert(QStringLiteral("sha256"), artifact.sha256.toLower());
    QSaveFile metadataFile(QDir(m_storageDirectory).filePath(artifact.id + QStringLiteral(".json")));
    if (!metadataFile.open(QIODevice::WriteOnly))
    {
        QFile::remove(finalPath);
        return std::unexpected(QStringLiteral("Unable to write resource-pack metadata."));
    }
    metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
    if (!metadataFile.commit())
    {
        QFile::remove(finalPath);
        return std::unexpected(QStringLiteral("Unable to commit resource-pack metadata."));
    }
    m_installedPacks.insert(
        artifact.id,
        {
            QFileInfo(finalPath).absoluteFilePath(),
            artifact.version,
            artifact.sha256.toLower()
        }
        );
    return {};
}

const ResourcePackManager::Definition* ResourcePackManager::definition(const QString& packId) const
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

Status ResourcePackManager::discoverInstalledPack(const Definition& packDefinition)
{
    if (!packDefinition.updateable)
    {
        return {};
    }
    QFile metadataFile(QDir(m_storageDirectory).filePath(packDefinition.id + QStringLiteral(".json")));
    if (!metadataFile.exists())
    {
        return {};
    }
    if (!metadataFile.open(QIODevice::ReadOnly))
    {
        return std::unexpected(QStringLiteral("Unable to read metadata for resource pack '%1'.").arg(packDefinition.id));
    }
    const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll());
    const QJsonObject metadata = document.object();
    const auto version = Version::parse(metadata.value(QStringLiteral("version")).toString());
    const QString fileName = metadata.value(QStringLiteral("fileName")).toString();
    const QString expectedHash = metadata.value(QStringLiteral("sha256")).toString().toLower();
    const auto baselineVersion = classmngr::engine::SemanticVersion::parse(
        packDefinition.baselineVersion.toString().toUtf8().toStdString()
        );
    const classmngr::engine::ResourcePackDefinition policyDefinition{
        packDefinition.id.toUtf8().toStdString(),
        baselineVersion ? *baselineVersion : classmngr::engine::SemanticVersion(),
        packDefinition.updateable
    };
    const classmngr::engine::InstalledResourcePackMetadata policyMetadata{
        metadata.value(QStringLiteral("schemaVersion")).toInt(-1),
        metadata.value(QStringLiteral("id")).toString().toUtf8().toStdString(),
        metadata.value(QStringLiteral("version")).toString().toUtf8().toStdString(),
        fileName.toUtf8().toStdString(), expectedHash.toUtf8().toStdString()
    };
    if (!document.isObject() || !baselineVersion)
    {
        return std::unexpected(QStringLiteral("Metadata for resource pack '%1' is invalid.").arg(packDefinition.id));
    }
    const auto validatedMetadata =
        classmngr::engine::ResourcePackPolicy::validateInstalledMetadata(
            policyMetadata,
            policyDefinition
            );
    if (!validatedMetadata)
    {
        return std::unexpected(QStringLiteral("Metadata for resource pack '%1' is invalid.").arg(packDefinition.id));
    }
    const QString normalizedFileName = QString::fromStdString(
        validatedMetadata->fileName
        );
    const QString normalizedHash = QString::fromStdString(
        validatedMetadata->sha256
        );
    const QString filePath = QDir(m_storageDirectory).filePath(normalizedFileName);
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.isFile())
    {
        return std::unexpected(QStringLiteral("Resource pack '%1' failed its integrity check.").arg(packDefinition.id));
    }
    m_installedPacks.insert(
        packDefinition.id,
        {fileInfo.absoluteFilePath(), *version, normalizedHash}
        );
    return {};
}

Status ResourcePackManager::validateInstalledPack(
    const QString& packId,
    const InstalledPack& pack
    ) const
{
    if (sha256ForFile(pack.filePath).compare(pack.expectedHash, Qt::CaseInsensitive) != 0)
    {
        return std::unexpected(QStringLiteral("Resource pack '%1' failed its integrity check.").arg(packId));
    }
    return {};
}

Status ResourcePackManager::mount(const Definition& packDefinition)
{
    if (m_mountedPacks.contains(packDefinition.id))
    {
        return {};
    }
    auto installed = m_installedPacks.constFind(packDefinition.id);
    const bool hasInstalled = installed != m_installedPacks.cend();
    const bool installedIntegrityValid = hasInstalled
        && validateInstalledPack(packDefinition.id, *installed).has_value();
    const auto baselineVersion = classmngr::engine::SemanticVersion::parse(
        packDefinition.baselineVersion.toString().toUtf8().toStdString()
        );
    std::optional<classmngr::engine::InstalledResourcePackMetadata>
        installedMetadata;
    if (hasInstalled)
    {
        installedMetadata = classmngr::engine::InstalledResourcePackMetadata{
            InstalledMetadataSchemaVersion,
            packDefinition.id.toUtf8().toStdString(),
            installed->version.toString().toUtf8().toStdString(),
            QFileInfo(installed->filePath).fileName().toUtf8().toStdString(),
            installed->expectedHash.toUtf8().toStdString()
        };
    }
    const classmngr::engine::ResourcePackSelection selection =
        classmngr::engine::ResourcePackPolicy::select(
            {
                packDefinition.id.toUtf8().toStdString(),
                baselineVersion
                    ? *baselineVersion
                    : classmngr::engine::SemanticVersion(),
                packDefinition.updateable
            },
            installedMetadata,
            installedIntegrityValid
            );
    if (selection.discardInstalled)
    {
        discardInstalledPack(packDefinition.id);
        installed = m_installedPacks.cend();
    }

    const bool useInstalled =
        selection.source == classmngr::engine::ResourcePackSource::Installed
        && installed != m_installedPacks.cend();

    const QString filePath = useInstalled
        ? installed->filePath
        : QDir(m_baselineDirectory).filePath(packDefinition.id + QStringLiteral(".rcc"));
    const Version version = useInstalled
        ? installed->version
        : packDefinition.baselineVersion;
    if (!QFileInfo(filePath).isFile())
    {
        return std::unexpected(QStringLiteral("Required resource pack '%1' is unavailable.").arg(packDefinition.id));
    }
    if (!QResource::registerResource(filePath))
    {
        return std::unexpected(QStringLiteral("Unable to mount resource pack '%1'.").arg(packDefinition.id));
    }
    const QString root = packRoot(packDefinition.id);
    if (!QDir(root).exists())
    {
        QResource::unregisterResource(filePath);
        return std::unexpected(QStringLiteral("Resource pack '%1' has the wrong root.").arg(packDefinition.id));
    }
    m_mountedPacks.insert(packDefinition.id, {QFileInfo(filePath).absoluteFilePath(), root, version, 0});
    MemoryUsageDiagnostics::recordEvent(
        QStringLiteral("resource-pack-mounted"),
        QStringLiteral("%1 (%2)")
            .arg(packDefinition.id, version.toString())
        );
    return {};
}

void ResourcePackManager::discardInstalledPack(
    const QString& packId
    )
{
    const auto installed = m_installedPacks.find(packId);
    if (installed == m_installedPacks.end())
    {
        return;
    }

    const QString filePath = installed->filePath;
    m_installedPacks.erase(installed);
    QFile::remove(filePath);
    QFile::remove(
        QDir(m_storageDirectory).filePath(packId + QStringLiteral(".json"))
        );
}

void ResourcePackManager::release(const QString& packId)
{
    auto mounted = m_mountedPacks.find(packId);
    if (mounted == m_mountedPacks.end())
    {
        return;
    }
    if (--mounted->leaseCount > 0)
    {
        return;
    }
    const QString filePath = mounted->filePath;
    m_mountedPacks.erase(mounted);
    QResource::unregisterResource(filePath);
    MemoryUsageDiagnostics::recordEvent(
        QStringLiteral("resource-pack-unmounted"),
        packId
        );
}

void ResourcePackManager::removeStalePackFiles() const
{
    QDir directory(m_storageDirectory);
    QStringList activeFiles;
    for (const InstalledPack& pack : m_installedPacks)
    {
        activeFiles.append(QFileInfo(pack.filePath).absoluteFilePath());
    }
    for (const QString& fileName : directory.entryList({QStringLiteral("*.rcc")}, QDir::Files))
    {
        const QString path = QFileInfo(directory.filePath(fileName)).absoluteFilePath();
        if (!activeFiles.contains(path))
        {
            QFile::remove(path);
        }
    }

    for (const QString& fileName : directory.entryList({QStringLiteral("*.json")}, QDir::Files))
    {
        const QString packId = QFileInfo(fileName).completeBaseName();
        if (!definition(packId))
        {
            QFile::remove(directory.filePath(fileName));
        }
    }
}
