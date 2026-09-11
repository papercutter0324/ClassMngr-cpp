#pragma once

#include "classmngr/engine/result.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

struct DocumentLocalizedNames
{
    std::string defaultName;
    std::map<std::string, std::string> localeNames;

    [[nodiscard]] std::string forLocale(
        std::string_view localeName
        ) const;
};

struct DocumentAssetReference
{
    std::string path;
    std::string fileName;
};

struct DocumentFolderDefinition
{
    std::string id;
    std::string path;
    std::string parentPath;
    int order = 0;
    DocumentLocalizedNames sidebarNames;
};

struct DocumentDefinition
{
    std::string id;
    std::string folderPath;
    int order = 0;
    DocumentLocalizedNames sidebarNames;
    DocumentAssetReference pdf;
    bool printingEnabled = false;
    bool exportingEnabled = false;
    std::optional<DocumentAssetReference> exportFile;
};

struct DocumentCatalogInput
{
    std::vector<DocumentFolderDefinition> folders;
    std::vector<DocumentDefinition> documents;
};

struct DocumentCatalogModel
{
    std::vector<DocumentFolderDefinition> folders;
    std::vector<DocumentDefinition> documents;
    std::vector<std::string> warnings;
};

enum class DocumentCatalogSource
{
    None,
    Active,
    Embedded
};

struct DocumentCatalogSelection
{
    DocumentCatalogSource source = DocumentCatalogSource::None;
    bool activeWasInvalid = false;
};

class DocumentCatalogService final
{
public:
    [[nodiscard]] static DocumentCatalogModel build(
        const DocumentCatalogInput& input
        );
    [[nodiscard]] static DocumentCatalogSelection selectSource(
        bool activeValid,
        bool embeddedValid
        );
};

[[nodiscard]] Result<std::string> normalizeRelativeDirectoryPath(
    std::string_view path
    );

[[nodiscard]] Result<std::string> validatePlainFileName(
    std::string_view fileName
    );

[[nodiscard]] Result<int> validateOrder(
    long long order
    );

[[nodiscard]] bool validIdentifier(
    std::string_view id
    );

[[nodiscard]] std::string parentPath(
    std::string_view path
    );

[[nodiscard]] Result<DocumentLocalizedNames> normalizeLocalizedNames(
    std::string_view defaultName,
    const std::map<std::string, std::string>& localeNames
    );

} // namespace classmngr::engine
