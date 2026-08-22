#include "document_catalog.h"

#include "core/resource_paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <limits>

namespace
{
constexpr int SupportedSchemaVersion = 1;

Result<QString> relativeDirectoryPath(
    const QJsonValue& value,
    const QString& fieldName
    )
{
    if (!value.isString())
    {
        return std::unexpected(
            QStringLiteral("%1 must be a string.").arg(fieldName)
            );
    }

    QString path =
        value.toString().trimmed();

    path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    if (
        path.isEmpty()
        || QDir::isAbsolutePath(path)
        || path.startsWith(QLatin1Char('/'))
        )
    {
        return std::unexpected(
            QStringLiteral("%1 must be a non-empty relative directory path.")
                .arg(fieldName)
            );
    }

    const QStringList parts =
        path.split(QLatin1Char('/'), Qt::KeepEmptyParts);

    if (
        parts.contains(QString())
        || parts.contains(QStringLiteral("."))
        || parts.contains(QStringLiteral(".."))
        )
    {
        return std::unexpected(
            QStringLiteral("%1 contains an unsafe path segment.")
                .arg(fieldName)
            );
    }

    const QString cleaned =
        QDir::cleanPath(path);

    if (
        cleaned == QStringLiteral(".")
        || cleaned.startsWith(QStringLiteral("../"))
        || cleaned.contains(QStringLiteral("/../"))
        )
    {
        return std::unexpected(
            QStringLiteral("%1 escapes the Documents root.")
                .arg(fieldName)
            );
    }

    return cleaned;
}

Result<QString> plainFileName(
    const QJsonValue& value,
    const QString& fieldName
    )
{
    if (!value.isString())
    {
        return std::unexpected(
            QStringLiteral("%1 must be a string.").arg(fieldName)
            );
    }

    const QString fileName =
        value.toString().trimmed();

    if (
        fileName.isEmpty()
        || fileName == QStringLiteral(".")
        || fileName == QStringLiteral("..")
        || fileName.contains(QLatin1Char('/'))
        || fileName.contains(QLatin1Char('\\'))
        || QFileInfo(fileName).fileName() != fileName
        )
    {
        return std::unexpected(
            QStringLiteral("%1 must be a plain file name.")
                .arg(fieldName)
            );
    }

    return fileName;
}

Result<int> orderValue(
    const QJsonValue& value,
    const QString& fieldName
    )
{
    if (!value.isDouble())
    {
        return std::unexpected(
            QStringLiteral("%1 must be an integer.").arg(fieldName)
            );
    }

    const qint64 order =
        value.toInteger(-1);

    if (order < 0 || order > std::numeric_limits<int>::max())
    {
        return std::unexpected(
            QStringLiteral("%1 must be a non-negative integer.")
                .arg(fieldName)
            );
    }

    return static_cast<int>(order);
}

Result<DocumentLocalizedNames> localizedNames(
    const QJsonValue& value,
    const QString& fieldName
    )
{
    if (!value.isObject())
    {
        return std::unexpected(
            QStringLiteral("%1 must be an object.").arg(fieldName)
            );
    }

    const QJsonObject object =
        value.toObject();

    const QString defaultName =
        object.value(QStringLiteral("default")).toString().trimmed();

    if (defaultName.isEmpty())
    {
        return std::unexpected(
            QStringLiteral("%1.default is required.").arg(fieldName)
            );
    }

    DocumentLocalizedNames names;
    names.defaultName =
        defaultName;

    for (auto it = object.constBegin(); it != object.constEnd(); ++it)
    {
        if (it.key() == QStringLiteral("default"))
        {
            continue;
        }

        if (!it.value().isString() || it.value().toString().trimmed().isEmpty())
        {
            return std::unexpected(
                QStringLiteral("%1.%2 must be a non-empty string.")
                    .arg(fieldName, it.key())
                );
        }

        QString localeName =
            it.key().trimmed();

        localeName.replace(QLatin1Char('-'), QLatin1Char('_'));

        if (localeName.isEmpty())
        {
            return std::unexpected(
                QStringLiteral("%1 contains an empty locale key.")
                    .arg(fieldName)
                );
        }

        names.localeNames.insert(
            localeName,
            it.value().toString().trimmed()
            );
    }

    return names;
}

Result<DocumentAssetReference> assetReference(
    const QJsonValue& value,
    const QString& fieldName,
    const QString& rootPath
    )
{
    if (!value.isObject())
    {
        return std::unexpected(
            QStringLiteral("%1 must be an object.").arg(fieldName)
            );
    }

    const QJsonObject object =
        value.toObject();

    const auto path =
        relativeDirectoryPath(
            object.value(QStringLiteral("path")),
            fieldName + QStringLiteral(".path")
            );
    if (!path)
    {
        return std::unexpected(path.error());
    }

    const auto fileName =
        plainFileName(
            object.value(QStringLiteral("fileName")),
            fieldName + QStringLiteral(".fileName")
            );
    if (!fileName)
    {
        return std::unexpected(fileName.error());
    }

    const QString directoryPath =
        QDir(rootPath).filePath(*path);

    if (!QDir(directoryPath).exists())
    {
        return std::unexpected(
            QStringLiteral("%1 directory does not exist: %2")
                .arg(fieldName, *path)
            );
    }

    const QString absoluteFilePath =
        QDir(directoryPath).filePath(*fileName);

    const QFileInfo fileInfo(absoluteFilePath);

    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        return std::unexpected(
            QStringLiteral("%1 file does not exist: %2/%3")
                .arg(fieldName, *path, *fileName)
            );
    }

    return DocumentAssetReference{
        *path,
        *fileName,
        absoluteFilePath
    };
}

QString parentPathFor(
    const QString& path
    )
{
    const int separator =
        path.lastIndexOf(QLatin1Char('/'));

    return separator < 0
        ? QString()
        : path.left(separator);
}

QString entryName(
    const QJsonObject& object,
    qsizetype index,
    const QString& kind
    )
{
    const QString id =
        object.value(QStringLiteral("id")).toString().trimmed();

    return id.isEmpty()
        ? QStringLiteral("%1[%2]").arg(kind).arg(index)
        : QStringLiteral("%1 '%2'").arg(kind, id);
}

bool validIdentifier(
    const QString& id
    )
{
    if (id.isEmpty())
    {
        return false;
    }

    for (const QChar character : id)
    {
        if (
            !character.isLetterOrNumber()
            && character != QLatin1Char('_')
            && character != QLatin1Char('-')
            )
        {
            return false;
        }
    }

    return true;
}
}

Result<DocumentCatalog> DocumentCatalog::loadCatalogRoot(
    const QString& rootPath
    )
{
    if (rootPath.trimmed().isEmpty() || !QDir(rootPath).exists())
    {
        return std::unexpected(
            QStringLiteral("Documents root is unavailable: %1")
                .arg(rootPath)
            );
    }

    const QString catalogPath =
        QDir(rootPath).filePath(QStringLiteral("documents.json"));

    QFile file(catalogPath);

    if (!file.open(QIODevice::ReadOnly))
    {
        return std::unexpected(
            QStringLiteral("Unable to read Documents catalog: %1")
                .arg(catalogPath)
            );
    }

    QJsonParseError parseError;
    const QJsonDocument jsonDocument =
        QJsonDocument::fromJson(
            file.readAll(),
            &parseError
            );

    if (parseError.error != QJsonParseError::NoError)
    {
        return std::unexpected(
            QStringLiteral("Documents catalog JSON is invalid: %1")
                .arg(parseError.errorString())
            );
    }

    if (!jsonDocument.isObject())
    {
        return std::unexpected(
            QStringLiteral("Documents catalog root must be an object.")
            );
    }

    const QJsonObject root =
        jsonDocument.object();

    if (
        !root.value(QStringLiteral("schemaVersion")).isDouble()
        || root.value(QStringLiteral("schemaVersion")).toInteger(-1)
            != SupportedSchemaVersion
        )
    {
        return std::unexpected(
            QStringLiteral("Unsupported Documents catalog schema version.")
            );
    }

    const QJsonValue foldersValue =
        root.value(QStringLiteral("folders"));
    const QJsonValue documentsValue =
        root.value(QStringLiteral("documents"));

    if (!foldersValue.isArray() || !documentsValue.isArray())
    {
        return std::unexpected(
            QStringLiteral("Documents catalog folders and documents arrays are required.")
            );
    }

    DocumentCatalog catalog;
    catalog.m_rootPath =
        rootPath;

    QSet<QString> folderIds;
    QSet<QString> folderPaths;

    const QJsonArray folderArray =
        foldersValue.toArray();

    for (qsizetype index = 0; index < folderArray.size(); ++index)
    {
        if (!folderArray.at(index).isObject())
        {
            catalog.m_warnings.append(
                QStringLiteral("folder[%1] must be an object and was skipped.")
                    .arg(index)
                );
            continue;
        }

        const QJsonObject object =
            folderArray.at(index).toObject();
        const QString name =
            entryName(object, index, QStringLiteral("folder"));
        const QString id =
            object.value(QStringLiteral("id")).toString().trimmed();

        const auto path =
            relativeDirectoryPath(
                object.value(QStringLiteral("path")),
                name + QStringLiteral(".path")
                );
        const auto order =
            orderValue(
                object.value(QStringLiteral("order")),
                name + QStringLiteral(".order")
                );
        const auto names =
            localizedNames(
                object.value(QStringLiteral("sidebarNames")),
                name + QStringLiteral(".sidebarNames")
                );

        QString error;

        if (!validIdentifier(id))
        {
            error =
                QStringLiteral("%1.id is invalid.").arg(name);
        }
        else if (!path)
        {
            error =
                path.error();
        }
        else if (!order)
        {
            error =
                order.error();
        }
        else if (!names)
        {
            error =
                names.error();
        }
        else if (folderIds.contains(id))
        {
            error =
                QStringLiteral("%1 duplicates folder id '%2'.")
                    .arg(name, id);
        }
        else if (folderPaths.contains(*path))
        {
            error =
                QStringLiteral("%1 duplicates folder path '%2'.")
                    .arg(name, *path);
        }
        else if (!QDir(QDir(rootPath).filePath(*path)).exists())
        {
            error =
                QStringLiteral("%1 directory does not exist: %2")
                    .arg(name, *path);
        }

        if (!error.isEmpty())
        {
            catalog.m_warnings.append(
                error + QStringLiteral(" The folder was skipped.")
                );
            continue;
        }

        folderIds.insert(id);
        folderPaths.insert(*path);
        catalog.m_folders.append({
            id,
            *path,
            parentPathFor(*path),
            *order,
            *names
        });
    }

    QSet<QString> reachableFolderPaths;
    bool changed = true;

    while (changed)
    {
        changed = false;

        for (const DocumentFolderDefinition& folder : catalog.m_folders)
        {
            if (
                reachableFolderPaths.contains(folder.path)
                || (
                    !folder.parentPath.isEmpty()
                    && !reachableFolderPaths.contains(folder.parentPath)
                    )
                )
            {
                continue;
            }

            reachableFolderPaths.insert(folder.path);
            changed = true;
        }
    }

    for (const DocumentFolderDefinition& folder : catalog.m_folders)
    {
        if (!reachableFolderPaths.contains(folder.path))
        {
            catalog.m_warnings.append(
                QStringLiteral(
                    "folder '%1' has no valid metadata for an ancestor and will not be displayed."
                    ).arg(folder.id)
                );
        }
    }

    QSet<QString> documentIds;
    const QJsonArray documentArray =
        documentsValue.toArray();

    for (qsizetype index = 0; index < documentArray.size(); ++index)
    {
        if (!documentArray.at(index).isObject())
        {
            catalog.m_warnings.append(
                QStringLiteral("document[%1] must be an object and was skipped.")
                    .arg(index)
                );
            continue;
        }

        const QJsonObject object =
            documentArray.at(index).toObject();
        const QString name =
            entryName(object, index, QStringLiteral("document"));
        const QString id =
            object.value(QStringLiteral("id")).toString().trimmed();

        const auto order =
            orderValue(
                object.value(QStringLiteral("order")),
                name + QStringLiteral(".order")
                );
        const auto names =
            localizedNames(
                object.value(QStringLiteral("sidebarNames")),
                name + QStringLiteral(".sidebarNames")
                );
        const auto pdf =
            assetReference(
                object.value(QStringLiteral("pdf")),
                name + QStringLiteral(".pdf"),
                rootPath
                );

        QString error;

        if (!validIdentifier(id))
        {
            error =
                QStringLiteral("%1.id is invalid.").arg(name);
        }
        else if (documentIds.contains(id))
        {
            error =
                QStringLiteral("%1 duplicates document id '%2'.")
                    .arg(name, id);
        }
        else if (!order)
        {
            error =
                order.error();
        }
        else if (!names)
        {
            error =
                names.error();
        }
        else if (!pdf)
        {
            error =
                pdf.error();
        }
        else if (
            QFileInfo(pdf->fileName).suffix().compare(
                QStringLiteral("pdf"),
                Qt::CaseInsensitive
                ) != 0
            )
        {
            error =
                QStringLiteral("%1.pdf.fileName must have a .pdf extension.")
                    .arg(name);
        }
        else if (!reachableFolderPaths.contains(pdf->path))
        {
            error =
                QStringLiteral("%1 references folder '%2' without valid metadata.")
                    .arg(name, pdf->path);
        }

        const QJsonValue printingValue =
            object.value(QStringLiteral("printingEnabled"));
        const QJsonValue exportingValue =
            object.value(QStringLiteral("exportingEnabled"));

        if (
            error.isEmpty()
            && (!printingValue.isBool() || !exportingValue.isBool())
            )
        {
            error =
                QStringLiteral(
                    "%1 printingEnabled and exportingEnabled must be booleans."
                    ).arg(name);
        }

        std::optional<DocumentAssetReference> exportFile;

        if (
            error.isEmpty()
            && exportingValue.toBool()
            )
        {
            const auto exportReference =
                assetReference(
                    object.value(QStringLiteral("export")),
                    name + QStringLiteral(".export"),
                    rootPath
                    );

            if (!exportReference)
            {
                error =
                    exportReference.error();
            }
            else
            {
                exportFile =
                    *exportReference;
            }
        }

        if (!error.isEmpty())
        {
            catalog.m_warnings.append(
                error + QStringLiteral(" The document was skipped.")
                );
            continue;
        }

        documentIds.insert(id);
        catalog.m_documents.append({
            id,
            pdf->path,
            *order,
            *names,
            *pdf,
            printingValue.toBool(),
            exportingValue.toBool(),
            exportFile
        });
    }

    catalog.m_folders.erase(
        std::remove_if(
            catalog.m_folders.begin(),
            catalog.m_folders.end(),
            [&reachableFolderPaths](const DocumentFolderDefinition& folder)
            {
                return !reachableFolderPaths.contains(folder.path);
            }
            ),
        catalog.m_folders.end()
        );

    catalog.buildDocumentIndex();

    return catalog;
}

QString DocumentLocalizedNames::forLocale(
    const QString& localeName
    ) const
{
    QString normalizedLocale =
        localeName.trimmed();

    normalizedLocale.replace(QLatin1Char('-'), QLatin1Char('_'));

    const auto exact =
        localeNames.constFind(normalizedLocale);

    if (exact != localeNames.constEnd())
    {
        return exact.value();
    }

    const QString language =
        normalizedLocale.section(QLatin1Char('_'), 0, 0);
    const auto languageMatch =
        localeNames.constFind(language);

    return languageMatch != localeNames.constEnd()
        ? languageMatch.value()
        : defaultName;
}

Status DocumentCatalog::initialize()
{
    auto lease = ResourcePaths::Documents::acquire();
    if (!lease)
    {
        m_rootPath.clear();
        m_folders.clear();
        m_documents.clear();
        m_documentIndexes.clear();
        m_warnings = {lease.error()};
        return std::unexpected(lease.error());
    }

    const auto catalog = loadFromRoot(
        ResourcePaths::Documents::directory(*lease)
        );

    if (!catalog)
    {
        m_rootPath.clear();
        m_folders.clear();
        m_documents.clear();
        m_documentIndexes.clear();
        m_warnings = {catalog.error()};
        return std::unexpected(catalog.error());
    }

    *this = *catalog;

    // The sidebar retains parsed metadata only. Resource paths are rebuilt
    // from a fresh documents lease when a document is actually opened.
    m_rootPath.clear();
    for (DocumentDefinition& document : m_documents)
    {
        document.pdf.absoluteFilePath.clear();
        if (document.exportFile)
        {
            document.exportFile->absoluteFilePath.clear();
        }
    }

    return {};
}

Result<DocumentCatalog> DocumentCatalog::loadFromRoot(
    const QString& rootPath
    )
{
    return loadCatalogRoot(rootPath);
}

Result<DocumentCatalog> DocumentCatalog::loadFromRoots(
    const QString& activeRootPath,
    const QString& embeddedRootPath
    )
{
    QString activeError;

    if (!activeRootPath.trimmed().isEmpty())
    {
        auto activeCatalog =
            loadCatalogRoot(activeRootPath);

        if (activeCatalog)
        {
            return activeCatalog;
        }

        activeError =
            activeCatalog.error();
    }

    auto embeddedCatalog =
        loadCatalogRoot(embeddedRootPath);

    if (!embeddedCatalog)
    {
        return std::unexpected(
            activeError.isEmpty()
                ? embeddedCatalog.error()
                : QStringLiteral(
                    "Active Documents catalog failed: %1\n"
                    "Embedded Documents catalog failed: %2"
                    ).arg(activeError, embeddedCatalog.error())
            );
    }

    if (!activeError.isEmpty())
    {
        embeddedCatalog->m_warnings.prepend(
            QStringLiteral(
                "Active Documents catalog failed and the embedded catalog was used: %1"
                ).arg(activeError)
            );
    }

    return embeddedCatalog;
}

const QList<DocumentFolderDefinition>& DocumentCatalog::folders() const
{
    return m_folders;
}

const QList<DocumentDefinition>& DocumentCatalog::documents() const
{
    return m_documents;
}

const DocumentDefinition* DocumentCatalog::document(
    const QString& id
    ) const
{
    const auto it =
        m_documentIndexes.constFind(id);

    return it == m_documentIndexes.constEnd()
        ? nullptr
        : &m_documents.at(it.value());
}

QString DocumentCatalog::rootPath() const
{
    return m_rootPath;
}

QStringList DocumentCatalog::warnings() const
{
    return m_warnings;
}

bool DocumentCatalog::isEmpty() const
{
    return m_documents.isEmpty();
}

void DocumentCatalog::buildDocumentIndex()
{
    m_documentIndexes.clear();

    for (int index = 0; index < m_documents.size(); ++index)
    {
        m_documentIndexes.insert(
            m_documents.at(index).id,
            index
            );
    }
}
