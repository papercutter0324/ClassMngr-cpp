#include "document_catalog.h"

#include "core/resource_paths.h"
#include "classmngr/engine/document_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

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

    const QByteArray utf8Path = value.toString().toUtf8();
    const auto path =
        classmngr::engine::normalizeRelativeDirectoryPath(
            std::string_view(utf8Path.constData(), utf8Path.size())
            );
    if (!path)
    {
        return std::unexpected(
            QStringLiteral("%1 %2")
                .arg(
                    fieldName,
                    QString::fromUtf8(
                        path.error().message.data(),
                        static_cast<qsizetype>(path.error().message.size())
                        )
                    )
            );
    }

    return QString::fromUtf8(
        path->data(),
        static_cast<qsizetype>(path->size())
        );
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

    const QByteArray utf8FileName = value.toString().toUtf8();
    const auto fileName =
        classmngr::engine::validatePlainFileName(
            std::string_view(
                utf8FileName.constData(),
                utf8FileName.size()
                )
            );
    if (!fileName)
    {
        return std::unexpected(
            QStringLiteral("%1 %2")
                .arg(
                    fieldName,
                    QString::fromUtf8(
                        fileName.error().message.data(),
                        static_cast<qsizetype>(
                            fileName.error().message.size()
                            )
                        )
                    )
            );
    }

    return QString::fromUtf8(
        fileName->data(),
        static_cast<qsizetype>(fileName->size())
        );
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

    const auto order =
        classmngr::engine::validateOrder(
            static_cast<long long>(value.toInteger(-1))
            );

    if (!order)
    {
        return std::unexpected(
            QStringLiteral("%1 %2")
                .arg(
                    fieldName,
                    QString::fromUtf8(
                        order.error().message.data(),
                        static_cast<qsizetype>(order.error().message.size())
                        )
                    )
            );
    }

    return *order;
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

    std::map<std::string, std::string> localeNameValues;

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

        const QString localeName = it.key().trimmed();

        if (localeName.isEmpty())
        {
            return std::unexpected(
                QStringLiteral("%1 contains an empty locale key.")
                    .arg(fieldName)
                );
        }

        const QByteArray localeUtf8 = localeName.toUtf8();
        const QByteArray localizedValueUtf8 =
            it.value().toString().trimmed().toUtf8();
        localeNameValues.insert_or_assign(
            std::string(localeUtf8.constData(), localeUtf8.size()),
            std::string(
                localizedValueUtf8.constData(),
                localizedValueUtf8.size()
                )
            );
    }

    const QByteArray defaultUtf8 = defaultName.toUtf8();
    const auto names =
        classmngr::engine::normalizeLocalizedNames(
            std::string_view(defaultUtf8.constData(), defaultUtf8.size()),
            localeNameValues
            );
    if (!names)
    {
        return std::unexpected(
            QStringLiteral("%1 %2")
                .arg(
                    fieldName,
                    QString::fromUtf8(
                        names.error().message.data(),
                        static_cast<qsizetype>(names.error().message.size())
                        )
                    )
            );
    }

    DocumentLocalizedNames result;
    result.defaultName = QString::fromUtf8(
        names->defaultName.data(),
        static_cast<qsizetype>(names->defaultName.size())
        );
    for (const auto& [locale, localizedValue] : names->localeNames)
    {
        result.localeNames.insert(
            QString::fromUtf8(
                locale.data(),
                static_cast<qsizetype>(locale.size())
                ),
            QString::fromUtf8(
                localizedValue.data(),
                static_cast<qsizetype>(localizedValue.size())
                )
            );
    }
    return result;
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
    const QByteArray utf8Path = path.toUtf8();
    const std::string result = classmngr::engine::parentPath(
        std::string_view(utf8Path.constData(), utf8Path.size())
        );
    return QString::fromUtf8(
        result.data(),
        static_cast<qsizetype>(result.size())
        );
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
    const QByteArray utf8Id = id.toUtf8();
    return classmngr::engine::validIdentifier(
        std::string_view(utf8Id.constData(), utf8Id.size())
        );
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

    classmngr::engine::DocumentCatalogInput engineInput;

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

        const QByteArray idUtf8 = id.toUtf8();
        const QByteArray pathUtf8 = path->toUtf8();
        const QByteArray parentPathUtf8 = parentPathFor(*path).toUtf8();
        const QByteArray defaultNameUtf8 = names->defaultName.toUtf8();
        engineInput.folders.push_back({
            std::string(idUtf8.constData(), idUtf8.size()),
            std::string(pathUtf8.constData(), pathUtf8.size()),
            std::string(
                parentPathUtf8.constData(),
                parentPathUtf8.size()
                ),
            *order,
            {
                std::string(
                    defaultNameUtf8.constData(),
                    defaultNameUtf8.size()
                    ),
                {}
            }
        });

        auto& engineNames = engineInput.folders.back().sidebarNames;
        for (
            auto it = names->localeNames.constBegin();
            it != names->localeNames.constEnd();
            ++it
            )
        {
            const QByteArray localeUtf8 = it.key().toUtf8();
            const QByteArray localizedValueUtf8 = it.value().toUtf8();
            engineNames.localeNames.emplace(
                std::string(localeUtf8.constData(), localeUtf8.size()),
                std::string(
                    localizedValueUtf8.constData(),
                    localizedValueUtf8.size()
                    )
                );
        }
    }

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

        const QByteArray idUtf8 = id.toUtf8();
        const QByteArray folderPathUtf8 = pdf->path.toUtf8();
        const QByteArray defaultNameUtf8 = names->defaultName.toUtf8();
        const QByteArray pdfPathUtf8 = pdf->path.toUtf8();
        const QByteArray pdfFileNameUtf8 = pdf->fileName.toUtf8();
        classmngr::engine::DocumentDefinition engineDocument{
            std::string(idUtf8.constData(), idUtf8.size()),
            std::string(folderPathUtf8.constData(), folderPathUtf8.size()),
            *order,
            {
                std::string(
                    defaultNameUtf8.constData(),
                    defaultNameUtf8.size()
                    ),
                {}
            },
            {
                std::string(
                    pdfPathUtf8.constData(),
                    pdfPathUtf8.size()
                    ),
                std::string(
                    pdfFileNameUtf8.constData(),
                    pdfFileNameUtf8.size()
                    )
            },
            printingValue.toBool(),
            exportingValue.toBool(),
            std::nullopt
        };

        for (
            auto it = names->localeNames.constBegin();
            it != names->localeNames.constEnd();
            ++it
            )
        {
            const QByteArray localeUtf8 = it.key().toUtf8();
            const QByteArray localizedValueUtf8 = it.value().toUtf8();
            engineDocument.sidebarNames.localeNames.emplace(
                std::string(localeUtf8.constData(), localeUtf8.size()),
                std::string(
                    localizedValueUtf8.constData(),
                    localizedValueUtf8.size()
                    )
                );
        }

        if (exportFile)
        {
            const QByteArray exportPathUtf8 = exportFile->path.toUtf8();
            const QByteArray exportFileNameUtf8 = exportFile->fileName.toUtf8();
            engineDocument.exportFile =
                classmngr::engine::DocumentAssetReference{
                    std::string(
                        exportPathUtf8.constData(),
                        exportPathUtf8.size()
                        ),
                    std::string(
                        exportFileNameUtf8.constData(),
                        exportFileNameUtf8.size()
                        )
                };
        }

        engineInput.documents.push_back(std::move(engineDocument));
    }

    const classmngr::engine::DocumentCatalogModel engineModel =
        classmngr::engine::DocumentCatalogService::build(engineInput);

    for (const std::string& warning : engineModel.warnings)
    {
        catalog.m_warnings.append(
            QString::fromUtf8(
                warning.data(),
                static_cast<qsizetype>(warning.size())
                )
            );
    }

    for (
        const classmngr::engine::DocumentFolderDefinition& folder
            : engineModel.folders
        )
    {
        DocumentLocalizedNames names;
        names.defaultName = QString::fromUtf8(
            folder.sidebarNames.defaultName.data(),
            static_cast<qsizetype>(folder.sidebarNames.defaultName.size())
            );
        for (const auto& [locale, localizedValue] : folder.sidebarNames.localeNames)
        {
            names.localeNames.insert(
                QString::fromUtf8(
                    locale.data(),
                    static_cast<qsizetype>(locale.size())
                    ),
                QString::fromUtf8(
                    localizedValue.data(),
                    static_cast<qsizetype>(localizedValue.size())
                    )
                );
        }

        catalog.m_folders.append({
            QString::fromUtf8(
                folder.id.data(),
                static_cast<qsizetype>(folder.id.size())
                ),
            QString::fromUtf8(
                folder.path.data(),
                static_cast<qsizetype>(folder.path.size())
                ),
            QString::fromUtf8(
                folder.parentPath.data(),
                static_cast<qsizetype>(folder.parentPath.size())
                ),
            folder.order,
            std::move(names)
        });
    }

    for (const classmngr::engine::DocumentDefinition& document : engineModel.documents)
    {
        DocumentLocalizedNames names;
        names.defaultName = QString::fromUtf8(
            document.sidebarNames.defaultName.data(),
            static_cast<qsizetype>(document.sidebarNames.defaultName.size())
            );
        for (const auto& [locale, localizedValue] : document.sidebarNames.localeNames)
        {
            names.localeNames.insert(
                QString::fromUtf8(
                    locale.data(),
                    static_cast<qsizetype>(locale.size())
                    ),
                QString::fromUtf8(
                    localizedValue.data(),
                    static_cast<qsizetype>(localizedValue.size())
                    )
                );
        }

        const QString pdfPath = QString::fromUtf8(
            document.pdf.path.data(),
            static_cast<qsizetype>(document.pdf.path.size())
            );
        const QString pdfFileName = QString::fromUtf8(
            document.pdf.fileName.data(),
            static_cast<qsizetype>(document.pdf.fileName.size())
            );
        DocumentAssetReference pdf{
            pdfPath,
            pdfFileName,
            QDir(QDir(rootPath).filePath(pdfPath)).filePath(pdfFileName)
        };

        std::optional<DocumentAssetReference> exportFile;
        if (document.exportFile)
        {
            const QString exportPath = QString::fromUtf8(
                document.exportFile->path.data(),
                static_cast<qsizetype>(document.exportFile->path.size())
                );
            const QString exportFileName = QString::fromUtf8(
                document.exportFile->fileName.data(),
                static_cast<qsizetype>(document.exportFile->fileName.size())
                );
            exportFile = DocumentAssetReference{
                exportPath,
                exportFileName,
                QDir(QDir(rootPath).filePath(exportPath)).filePath(exportFileName)
            };
        }

        catalog.m_documents.append({
            QString::fromUtf8(
                document.id.data(),
                static_cast<qsizetype>(document.id.size())
                ),
            pdfPath,
            document.order,
            std::move(names),
            std::move(pdf),
            document.printingEnabled,
            document.exportingEnabled,
            std::move(exportFile)
        });
    }

    catalog.buildDocumentIndex();

    return catalog;
}

QString DocumentLocalizedNames::forLocale(
    const QString& localeName
    ) const
{
    classmngr::engine::DocumentLocalizedNames engineNames;
    const QByteArray defaultUtf8 = defaultName.toUtf8();
    engineNames.defaultName = std::string(
        defaultUtf8.constData(),
        defaultUtf8.size()
        );
    for (auto it = localeNames.constBegin(); it != localeNames.constEnd(); ++it)
    {
        const QByteArray localeUtf8 = it.key().toUtf8();
        const QByteArray localizedValueUtf8 = it.value().toUtf8();
        engineNames.localeNames.emplace(
            std::string(localeUtf8.constData(), localeUtf8.size()),
            std::string(
                localizedValueUtf8.constData(),
                localizedValueUtf8.size()
                )
            );
    }

    const QByteArray localeUtf8 = localeName.toUtf8();
    const std::string result = engineNames.forLocale(
        std::string_view(localeUtf8.constData(), localeUtf8.size())
        );
    return QString::fromUtf8(
        result.data(),
        static_cast<qsizetype>(result.size())
        );
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

    std::optional<DocumentCatalog> activeCatalog;
    if (!activeRootPath.trimmed().isEmpty())
    {
        auto loadedActiveCatalog = loadCatalogRoot(activeRootPath);
        if (loadedActiveCatalog)
        {
            activeCatalog = std::move(*loadedActiveCatalog);
        }
        else
        {
            activeError = loadedActiveCatalog.error();
        }
    }

    auto embeddedCatalog =
        loadCatalogRoot(embeddedRootPath);

    const auto selection = classmngr::engine::DocumentCatalogService::selectSource(
        activeCatalog.has_value(),
        embeddedCatalog.has_value()
        );
    if (selection.source == classmngr::engine::DocumentCatalogSource::Active)
    {
        return std::move(*activeCatalog);
    }

    if (selection.source == classmngr::engine::DocumentCatalogSource::None)
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
