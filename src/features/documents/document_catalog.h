#pragma once

#include "core/result.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

struct DocumentLocalizedNames
{
    QString defaultName;
    QHash<QString, QString> localeNames;

    [[nodiscard]] QString forLocale(
        const QString& localeName
        ) const;
};

struct DocumentAssetReference
{
    QString path;
    QString fileName;
    QString absoluteFilePath;
};

struct DocumentFolderDefinition
{
    QString id;
    QString path;
    QString parentPath;
    int order = 0;
    DocumentLocalizedNames sidebarNames;
};

struct DocumentDefinition
{
    QString id;
    QString folderPath;
    int order = 0;
    DocumentLocalizedNames sidebarNames;
    DocumentAssetReference pdf;
    bool printingEnabled = false;
    bool exportingEnabled = false;
    std::optional<DocumentAssetReference> exportFile;
};

class DocumentCatalog
{
public:
    [[nodiscard]] Status initialize();

    [[nodiscard]] static Result<DocumentCatalog> loadFromRoot(
        const QString& rootPath
        );

    [[nodiscard]] static Result<DocumentCatalog> loadFromRoots(
        const QString& activeRootPath,
        const QString& embeddedRootPath
        );

    [[nodiscard]] const QList<DocumentFolderDefinition>& folders() const;
    [[nodiscard]] const QList<DocumentDefinition>& documents() const;
    [[nodiscard]] const DocumentDefinition* document(
        const QString& id
        ) const;
    [[nodiscard]] QString rootPath() const;
    [[nodiscard]] QStringList warnings() const;
    [[nodiscard]] bool isEmpty() const;

private:
    [[nodiscard]] static Result<DocumentCatalog> loadCatalogRoot(
        const QString& rootPath
        );

    void buildDocumentIndex();

    QString m_rootPath;
    QList<DocumentFolderDefinition> m_folders;
    QList<DocumentDefinition> m_documents;
    QHash<QString, int> m_documentIndexes;
    QStringList m_warnings;
};
