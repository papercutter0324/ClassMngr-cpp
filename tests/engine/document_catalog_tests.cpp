#include "classmngr/engine/document_catalog.h"

#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using namespace classmngr::engine;

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineDocumentCatalogTests: "
              << message
              << '\n';
    return false;
}

DocumentLocalizedNames names(
    std::string defaultName,
    std::map<std::string, std::string> localeNames = {}
    )
{
    const auto normalized = normalizeLocalizedNames(defaultName, localeNames);
    return normalized ? *normalized : DocumentLocalizedNames{};
}

DocumentFolderDefinition folder(
    std::string id,
    std::string path,
    int order,
    std::string parent = {}
    )
{
    return {
        std::move(id),
        std::move(path),
        std::move(parent),
        order,
        names("폴더")
    };
}

DocumentDefinition document(
    std::string id,
    std::string folderPath,
    int order
    )
{
    return {
        std::move(id),
        std::move(folderPath),
        order,
        names("문서", {{"ko_KR", "문서"}}),
        {"Guides", "Guide.pdf"},
        true,
        false,
        std::nullopt
    };
}
} // namespace

int main()
{
    bool passed = true;

    const auto localized = normalizeLocalizedNames(
        " Default 이름 ",
        {
            {"ko", " 한국어 "},
            {"en-US", "English"}
        }
        );
    passed &= expect(localized.has_value(), "localized names were rejected");
    if (localized)
    {
        passed &= expect(
            localized->forLocale(" en-US ") == "English"
                && localized->forLocale("ko-KR") == "한국어"
                && localized->forLocale("fr-FR") == "Default 이름",
            "locale exact, language, or default fallback changed"
            );
    }

    passed &= expect(
        normalizeRelativeDirectoryPath(" Guides\\Advanced ").value()
            == "Guides/Advanced"
            && normalizeRelativeDirectoryPath("Guides").has_value(),
        "safe relative directory paths were not normalized"
        );
    passed &= expect(
        !normalizeRelativeDirectoryPath("").has_value()
            && !normalizeRelativeDirectoryPath("/outside").has_value()
            && !normalizeRelativeDirectoryPath("C:/outside").has_value()
            && !normalizeRelativeDirectoryPath("Guides//Advanced").has_value()
            && !normalizeRelativeDirectoryPath("Guides/./Advanced").has_value()
            && !normalizeRelativeDirectoryPath("Guides/../outside").has_value(),
        "unsafe relative directory paths were accepted"
        );

    passed &= expect(
        validatePlainFileName(" Guide.pdf ").value() == "Guide.pdf"
            && !validatePlainFileName("").has_value()
            && !validatePlainFileName(".").has_value()
            && !validatePlainFileName("../Guide.pdf").has_value()
            && !validatePlainFileName("Guides/Guide.pdf").has_value()
            && !validatePlainFileName("Guides\\Guide.pdf").has_value(),
        "plain filename acceptance or rejection changed"
        );
    passed &= expect(
        validateOrder(0).value() == 0
            && validateOrder(std::numeric_limits<int>::max()).value()
                == std::numeric_limits<int>::max()
            && !validateOrder(-1).has_value()
            && !validateOrder(
                static_cast<long long>(std::numeric_limits<int>::max()) + 1
                ).has_value(),
        "order validation changed"
        );
    passed &= expect(
        validIdentifier("document_한국-1")
            && !validIdentifier("")
            && !validIdentifier("document id")
            && !validIdentifier("document/id"),
        "identifier validation changed"
        );
    passed &= expect(
        parentPath("Guides/Advanced") == "Guides"
            && parentPath("Guides") == "",
        "parent path extraction changed"
        );
    passed &= expect(
        DocumentCatalogService::selectSource(true, true).source
                == DocumentCatalogSource::Active
            && DocumentCatalogService::selectSource(false, true).source
                == DocumentCatalogSource::Embedded
            && DocumentCatalogService::selectSource(false, false).source
                == DocumentCatalogSource::None,
        "active/embedded catalog fallback policy changed"
        );

    DocumentCatalogInput input;
    input.folders = {
        folder("root", "Guides", 20),
        folder("child", "Guides/Advanced", 10, "Guides"),
        folder("duplicate-id", "Other", 30),
        folder("duplicate-id", "Other-2", 40),
        folder("duplicate-path", "Other", 50),
        folder("unreachable", "Missing/Child", 60, "Missing")
    };
    input.documents = {
        document("first", "Guides/Advanced", 20),
        document("duplicate", "Guides", 10),
        document("duplicate", "Guides", 20),
        document("unreachable", "Missing/Child", 30),
        document("second", "Guides", 40)
    };

    const DocumentCatalogModel model = DocumentCatalogService::build(input);
    passed &= expect(
        model.folders.size() == 3
            && model.folders[0].id == "root"
            && model.folders[1].id == "child"
            && model.folders[2].id == "duplicate-id"
            && model.documents.size() == 3
            && model.documents[0].id == "first"
            && model.documents[1].id == "duplicate"
            && model.documents[2].id == "second",
        "duplicate or unreachable entries changed output order"
        );
    passed &= expect(
        model.warnings.size() == 5
            && model.warnings[0].find("duplicates folder id")
                != std::string::npos
            && model.warnings[1].find("duplicates folder path")
                != std::string::npos
            && model.warnings[2].find("no valid metadata")
                != std::string::npos
            && model.warnings[3].find("duplicates document id")
                != std::string::npos
            && model.warnings[4].find("without valid metadata")
                != std::string::npos,
        "duplicate or reachability warnings changed"
        );

    return passed ? 0 : 1;
}
