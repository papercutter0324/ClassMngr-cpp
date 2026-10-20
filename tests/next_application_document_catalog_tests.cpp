#include "next/application/document_catalog_projection.h"
#include "next/domain/document_content_reference.h"

#include <QtTest/QtTest>

#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Domain::DocumentContentReference;

template <typename Left, typename Right>
concept EqualityComparablePair = requires(
    const Left& left,
    const Right& right
    )
{
    left == right;
};

static_assert(std::is_same_v<
    decltype(DocumentFolderMetadata::path),
    DocumentCatalogPath
    >);
static_assert(std::is_same_v<
    decltype(DocumentFolderMetadata::parentPath),
    DocumentCatalogPath
    >);
static_assert(std::is_same_v<
    decltype(DocumentEntryMetadata::path),
    DocumentCatalogPath
    >);
static_assert(std::is_same_v<
    decltype(DocumentFolderMetadata::order),
    DocumentFolderOrder
    >);
static_assert(std::is_same_v<
    decltype(DocumentEntryMetadata::order),
    DocumentOrder
    >);
static_assert(std::is_same_v<
    decltype(DocumentFolderMetadata::key),
    DocumentFolderKey
    >);
static_assert(std::is_same_v<
    decltype(DocumentEntryMetadata::key),
    DocumentEntryKey
    >);
static_assert(!std::is_same_v<DocumentFolderKey, DocumentEntryKey>);
static_assert(std::is_same_v<DocumentFolderKey::Text, std::string>);
static_assert(std::is_same_v<DocumentEntryKey::Text, std::string>);
static_assert(std::is_same_v<
    decltype(std::declval<const DocumentFolderKey&>().value()),
    const std::string&
    >);
static_assert(std::is_same_v<
    decltype(std::declval<const DocumentEntryKey&>().value()),
    const std::string&
    >);
static_assert(std::is_constructible_v<DocumentFolderKey, std::string>);
static_assert(std::is_constructible_v<DocumentEntryKey, std::string>);
static_assert(!std::is_convertible_v<std::string, DocumentFolderKey>);
static_assert(!std::is_convertible_v<std::string, DocumentEntryKey>);
static_assert(!std::is_convertible_v<DocumentFolderKey, std::string>);
static_assert(!std::is_convertible_v<DocumentEntryKey, std::string>);
static_assert(!std::is_constructible_v<DocumentFolderKey, DocumentEntryKey>);
static_assert(!std::is_constructible_v<DocumentEntryKey, DocumentFolderKey>);
static_assert(!std::is_convertible_v<DocumentFolderKey, DocumentEntryKey>);
static_assert(!std::is_convertible_v<DocumentEntryKey, DocumentFolderKey>);
static_assert(!EqualityComparablePair<DocumentFolderKey, DocumentEntryKey>);
static_assert(!EqualityComparablePair<DocumentEntryKey, DocumentFolderKey>);
static_assert(!std::is_same_v<DocumentFolderOrder, DocumentOrder>);
static_assert(std::is_constructible_v<DocumentFolderOrder, std::int32_t>);
static_assert(std::is_constructible_v<DocumentOrder, std::int32_t>);
static_assert(!std::is_convertible_v<int, DocumentFolderOrder>);
static_assert(!std::is_convertible_v<int, DocumentOrder>);
static_assert(!std::is_convertible_v<std::int32_t, DocumentFolderOrder>);
static_assert(!std::is_convertible_v<std::int32_t, DocumentOrder>);
static_assert(!std::is_convertible_v<DocumentFolderOrder, DocumentOrder>);
static_assert(!std::is_convertible_v<DocumentOrder, DocumentFolderOrder>);

namespace
{

DocumentFolderId folderId(
    const std::string& value
    )
{
    return *DocumentFolderId::fromString(value);
}

DocumentId documentId(
    const std::string& value
    )
{
    return *DocumentId::fromString(value);
}

DocumentCatalogProjectionInput validInput()
{
    DocumentCatalogProjectionInput input;
    input.folders.push_back({
        folderId("folder-guides"),
        DocumentCatalogPath("Guides"),
        DocumentFolderKey("guides"),
        "Guides",
        DocumentFolderOrder(10)
    });
    input.documents.push_back({
        documentId("document-lesson-planning"),
        input.folders.front().id,
        DocumentCatalogPath("Guides/lesson-planning.pdf"),
        DocumentEntryKey("lesson-planning"),
        "Lesson Planning",
        DocumentOrder(20),
        true,
        false,
        DocumentContentReference("resource://documents/lesson-planning.pdf"),
        std::nullopt
    });
    return input;
}

void verifyInvalid(
    const Domain::Result<DocumentCatalogProjection>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
}

void verifyInvalidWithMessage(
    const Domain::Result<DocumentCatalogProjection>& result,
    const std::string& expectedMessage
    )
{
    verifyInvalid(result);
    QCOMPARE(result.error().message, expectedMessage);
    QVERIFY(!result.error().recoverable);
}

}

class NextApplicationDocumentCatalogTests final : public QObject
{
    Q_OBJECT

private slots:
    void validConstructionPreservesMetadataAndOrdering();
    void keyTypesRemainDistinctAndPreserveExactBytes();
    void matchingFolderAndEntryKeyTextIsAllowed();
    void orderTypesRemainDistinctAndDefaultToZero();
    void projectionPreservesInputSequenceIndependentOfOrderRanks();
    void completeMetadataIsPreserved();
    void typedFolderAndDocumentCategoriesRemainDistinct();
    void boundedValidationAndDuplicateIdsAreRejected();
    void folderPathsMustBeUniqueAndHierarchyMustBeRooted();
    void validNestedFoldersArePreserved();
    void unicodeWhitespaceIsRejectedAcrossBlankValidatedFields();
    void nonBlankUnicodeAndMalformedUtf8ArePreserved();
    void documentCollectionOverflowIsRejected();
    void negativeOrdersAreRejected();
    void optionalExportReferencePreservesAbsenceAndPresence();
    void exportReferenceBoundsAreRejected();
    void projectionsAreCopyableEqualAndIndependentlyReleasable();
    void contractHasOnlyAdapterNeutralMetadata();
};

void NextApplicationDocumentCatalogTests::validConstructionPreservesMetadataAndOrdering()
{
    const auto input = validInput();
    const auto result = DocumentCatalogProjection::create(input);

    QVERIFY(result);
    const DocumentCatalogProjection projection = result.value();
    QCOMPARE(projection.folders().size(), std::size_t(1));
    QCOMPARE(projection.documents().size(), std::size_t(1));
    QCOMPARE(projection.folders().front().id.value(), std::string("folder-guides"));
    QCOMPARE(projection.folders().front().path.value(), std::string("Guides"));
    QCOMPARE(projection.folders().front().parentPath.value(), std::string());
    QCOMPARE(projection.folders().front().key.value(), std::string("guides"));
    QCOMPARE(projection.folders().front().displayName, std::string("Guides"));
    QCOMPARE(projection.folders().front().order.rawValue(), std::int32_t(10));

    const auto& document = projection.documents().front();
    QCOMPARE(document.id.value(), std::string("document-lesson-planning"));
    QCOMPARE(document.folderId.value(), std::string("folder-guides"));
    QCOMPARE(document.path.value(), std::string("Guides/lesson-planning.pdf"));
    QCOMPARE(document.key.value(), std::string("lesson-planning"));
    QCOMPARE(document.displayName, std::string("Lesson Planning"));
    QCOMPARE(document.order.rawValue(), std::int32_t(20));
    QVERIFY(document.printable);
    QVERIFY(!document.exportable);
    QCOMPARE(
        document.contentReference.value(),
        std::string("resource://documents/lesson-planning.pdf")
        );
    QVERIFY(!document.exportReference.has_value());
    QVERIFY(projection.entries() == projection.documents());
}

void NextApplicationDocumentCatalogTests::orderTypesRemainDistinctAndDefaultToZero()
{
    const DocumentFolderMetadata folder{
        folderId("folder-default-order"),
        DocumentCatalogPath("Default"),
        DocumentFolderKey("default"),
        "Default"
    };
    const DocumentEntryMetadata document{
        .id = documentId("document-default-order"),
        .folderId = folderId("folder-default-order"),
        .path = DocumentCatalogPath("Default/document.pdf"),
        .key = DocumentEntryKey("default"),
        .displayName = "Default",
        .printable = true,
        .contentReference = DocumentContentReference(
            "resource://documents/Default/document.pdf"
            )
    };
    const DocumentFolderOrder folderOrder(std::int32_t(7));
    const DocumentOrder documentOrder(std::int32_t(7));

    QCOMPARE(folder.order.rawValue(), std::int32_t(0));
    QCOMPARE(document.order.rawValue(), std::int32_t(0));
    QCOMPARE(folderOrder.rawValue(), std::int32_t(7));
    QCOMPARE(documentOrder.rawValue(), std::int32_t(7));
    QVERIFY(folderOrder == DocumentFolderOrder(std::int32_t(7)));
    QVERIFY(documentOrder == DocumentOrder(std::int32_t(7)));
}

void NextApplicationDocumentCatalogTests::keyTypesRemainDistinctAndPreserveExactBytes()
{
    const DocumentFolderKey emptyFolderKey;
    const DocumentEntryKey emptyEntryKey;
    QCOMPARE(emptyFolderKey.value(), std::string());
    QCOMPARE(emptyEntryKey.value(), std::string());

    std::string exactFolderText = "folder";
    exactFolderText.push_back('\0');
    exactFolderText.append("\xFF", 1);
    exactFolderText.append("key");
    const DocumentFolderKey folderKey(exactFolderText);
    const DocumentFolderKey equalFolderKey(exactFolderText);
    std::string exactEntryText = "entry";
    exactEntryText.push_back('\0');
    exactEntryText.append("\xFE", 1);
    exactEntryText.append("key");
    const DocumentEntryKey entryKey(exactEntryText);
    const DocumentEntryKey equalEntryKey(exactEntryText);

    QCOMPARE(folderKey.value(), exactFolderText);
    QCOMPARE(entryKey.value(), exactEntryText);
    QVERIFY(folderKey == equalFolderKey);
    QVERIFY(entryKey == equalEntryKey);
    QVERIFY(folderKey != DocumentFolderKey("different"));
    QVERIFY(entryKey != DocumentEntryKey("different"));
}

void NextApplicationDocumentCatalogTests::matchingFolderAndEntryKeyTextIsAllowed()
{
    auto input = validInput();
    input.folders.front().key = DocumentFolderKey("shared-key");
    input.documents.front().key = DocumentEntryKey("shared-key");

    const auto result = DocumentCatalogProjection::create(std::move(input));

    QVERIFY(result);
    QCOMPARE(
        result.value().folders().front().key.value(),
        std::string("shared-key")
        );
    QCOMPARE(
        result.value().documents().front().key.value(),
        std::string("shared-key")
        );
}

void NextApplicationDocumentCatalogTests::
projectionPreservesInputSequenceIndependentOfOrderRanks()
{
    auto input = validInput();
    input.folders.front().id = folderId("folder-zulu");
    input.folders.front().path = DocumentCatalogPath("Zulu");
    input.folders.front().key = DocumentFolderKey("zulu");
    input.folders.front().displayName = "Zulu";
    input.folders.front().order = DocumentFolderOrder(20);
    input.documents.front().folderId = input.folders.front().id;
    input.documents.front().id = documentId("document-zulu");
    input.documents.front().path = DocumentCatalogPath("Zulu/zulu.pdf");
    input.documents.front().key = DocumentEntryKey("zulu");
    input.documents.front().displayName = "Zulu";
    input.documents.front().order = DocumentOrder(10);
    input.documents.front().contentReference = DocumentContentReference(
        "resource://documents/Zulu/zulu.pdf"
        );

    input.folders.push_back({
        folderId("folder-alpha"),
        DocumentCatalogPath("Alpha"),
        DocumentFolderKey("alpha"),
        "Alpha",
        DocumentFolderOrder(0)
    });
    input.folders.push_back({
        folderId("folder-mike"),
        DocumentCatalogPath("Mike"),
        DocumentFolderKey("mike"),
        "Mike",
        DocumentFolderOrder(10)
    });
    input.documents.push_back({
        documentId("document-alpha"),
        input.folders[1].id,
        DocumentCatalogPath("Alpha/alpha.pdf"),
        DocumentEntryKey("alpha"),
        "Alpha",
        DocumentOrder(0),
        false,
        false,
        DocumentContentReference("resource://documents/Alpha/alpha.pdf"),
        std::nullopt
    });
    input.documents.push_back({
        documentId("document-mike"),
        input.folders[2].id,
        DocumentCatalogPath("Mike/mike.pdf"),
        DocumentEntryKey("mike"),
        "Mike",
        DocumentOrder(5),
        true,
        false,
        DocumentContentReference("resource://documents/Mike/mike.pdf"),
        std::nullopt
    });

    const auto result = DocumentCatalogProjection::create(std::move(input));
    QVERIFY(result);
    QCOMPARE(result.value().folders().size(), std::size_t(3));
    QCOMPARE(result.value().documents().size(), std::size_t(3));

    const auto& folders = result.value().folders();
    QCOMPARE(folders[0].id.value(), std::string("folder-zulu"));
    QCOMPARE(folders[1].id.value(), std::string("folder-alpha"));
    QCOMPARE(folders[2].id.value(), std::string("folder-mike"));
    QCOMPARE(folders[0].order.rawValue(), std::int32_t(20));
    QCOMPARE(folders[1].order.rawValue(), std::int32_t(0));
    QCOMPARE(folders[2].order.rawValue(), std::int32_t(10));

    const auto& documents = result.value().documents();
    QCOMPARE(documents[0].id.value(), std::string("document-zulu"));
    QCOMPARE(documents[1].id.value(), std::string("document-alpha"));
    QCOMPARE(documents[2].id.value(), std::string("document-mike"));
    QCOMPARE(documents[0].order.rawValue(), std::int32_t(10));
    QCOMPARE(documents[1].order.rawValue(), std::int32_t(0));
    QCOMPARE(documents[2].order.rawValue(), std::int32_t(5));
}

void NextApplicationDocumentCatalogTests::completeMetadataIsPreserved()
{
    auto input = validInput();
    auto& folder = input.folders.front();
    folder.id = folderId("folder-curriculum");
    folder.path = DocumentCatalogPath("Catalog/Curriculum");
    folder.parentPath = DocumentCatalogPath("Catalog");
    folder.key = DocumentFolderKey("curriculum");
    folder.displayName = "Curriculum";
    folder.order = DocumentFolderOrder(42);
    const auto curriculumFolderId = folder.id;

    auto parent = folder;
    parent.id = folderId("folder-catalog");
    parent.path = DocumentCatalogPath("Catalog");
    parent.parentPath = DocumentCatalogPath();
    parent.key = DocumentFolderKey("catalog");
    parent.displayName = "Catalog";
    input.folders.push_back(std::move(parent));

    auto& document = input.documents.front();
    document.id = documentId("document-curriculum-guide");
    document.folderId = curriculumFolderId;
    document.path = DocumentCatalogPath("Catalog/Curriculum/guide.pdf");
    document.key = DocumentEntryKey("curriculum-guide");
    document.displayName = "Curriculum Guide";
    document.order = DocumentOrder(7);
    document.printable = false;
    document.exportable = true;
    document.contentReference = DocumentContentReference(
        "resource://documents/curriculum-guide.pdf"
        );
    document.exportReference = DocumentContentReference(
        "resource://documents/curriculum-guide.pptx"
        );

    const auto result = DocumentCatalogProjection::create(std::move(input));
    QVERIFY(result);

    const auto& projectedFolder = result.value().folders().front();
    QCOMPARE(projectedFolder.id.value(), std::string("folder-curriculum"));
    QCOMPARE(projectedFolder.path.value(), std::string("Catalog/Curriculum"));
    QCOMPARE(projectedFolder.parentPath.value(), std::string("Catalog"));
    QCOMPARE(projectedFolder.key.value(), std::string("curriculum"));
    QCOMPARE(projectedFolder.displayName, std::string("Curriculum"));
    QCOMPARE(projectedFolder.order.rawValue(), std::int32_t(42));

    const auto& projectedDocument = result.value().documents().front();
    QCOMPARE(
        projectedDocument.id.value(),
        std::string("document-curriculum-guide")
        );
    QCOMPARE(
        projectedDocument.folderId.value(),
        std::string("folder-curriculum")
        );
    QCOMPARE(
        projectedDocument.path.value(),
        std::string("Catalog/Curriculum/guide.pdf")
        );
    QCOMPARE(projectedDocument.key.value(), std::string("curriculum-guide"));
    QCOMPARE(
        projectedDocument.displayName,
        std::string("Curriculum Guide")
        );
    QCOMPARE(projectedDocument.order.rawValue(), std::int32_t(7));
    QVERIFY(!projectedDocument.printable);
    QVERIFY(projectedDocument.exportable);
    QCOMPARE(
        projectedDocument.contentReference.value(),
        std::string("resource://documents/curriculum-guide.pdf")
        );
    QVERIFY(projectedDocument.exportReference.has_value());
    QCOMPARE(
        projectedDocument.exportReference->value(),
        std::string("resource://documents/curriculum-guide.pptx")
        );
}

void NextApplicationDocumentCatalogTests::typedFolderAndDocumentCategoriesRemainDistinct()
{
    static_assert(!std::is_same_v<DocumentId, DocumentFolderId>);
    static_assert(!std::is_convertible_v<DocumentId, DocumentFolderId>);
    static_assert(!std::is_convertible_v<DocumentFolderId, DocumentId>);

    QVERIFY(!DocumentId::fromString("").has_value());
    QVERIFY(!DocumentFolderId::fromString("").has_value());

    const auto input = validInput();
    const auto result = DocumentCatalogProjection::create(input);
    QVERIFY(result);

    // The relationship is carried by the folder-specific identifier, not a
    // string that could silently accept the other identifier category.
    QCOMPARE(
        result.value().documents().front().folderId.value(),
        result.value().folders().front().id.value()
        );
}

void NextApplicationDocumentCatalogTests::boundedValidationAndDuplicateIdsAreRejected()
{
    {
        auto input = validInput();
        input.folders.push_back(input.folders.front());
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.documents.push_back(input.documents.front());
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.documents.front().folderId = folderId("unknown-folder");
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().id = folderId("   ");
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().path = DocumentCatalogPath("\t\n");
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().parentPath = DocumentCatalogPath(" \t\n");
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().parentPath = DocumentCatalogPath(std::string(
            kDocumentCatalogMaxPathLength + 1,
            'p'
            ));
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().path = DocumentCatalogPath(std::string(
            kDocumentCatalogMaxPathLength + 1,
            'f'
            ));
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.documents.front().path = DocumentCatalogPath(std::string(
            kDocumentCatalogMaxPathLength + 1,
            'd'
            ));
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().path = DocumentCatalogPath(std::string(
            kDocumentCatalogMaxPathLength,
            'f'
            ));
        const auto result = DocumentCatalogProjection::create(std::move(input));
        QVERIFY(result);
        QCOMPARE(
            result.value().folders().front().path.value().size(),
            kDocumentCatalogMaxPathLength
            );
    }

    {
        auto input = validInput();
        input.documents.front().path = DocumentCatalogPath(std::string(
            kDocumentCatalogMaxPathLength,
            'd'
            ));
        const auto result = DocumentCatalogProjection::create(std::move(input));
        QVERIFY(result);
        QCOMPARE(
            result.value().documents().front().path.value().size(),
            kDocumentCatalogMaxPathLength
            );
    }

    {
        auto input = validInput();
        input.folders.front().key = DocumentFolderKey(std::string(
            kDocumentCatalogMaxKeyLength + 1,
            'k'
            ));
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.documents.front().key = DocumentEntryKey(std::string(
            kDocumentCatalogMaxKeyLength + 1,
            'k'
            ));
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.documents.front().displayName = std::string(
            kDocumentCatalogMaxDisplayNameLength + 1,
            'd'
            );
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.documents.front().contentReference = DocumentContentReference(
            std::string(kDocumentCatalogMaxReferenceLength + 1, 'r')
            );
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        DocumentCatalogProjectionInput input;
        input.folders.reserve(kDocumentCatalogMaxFolderEntries + 1);
        for (std::size_t index = 0;
             index < kDocumentCatalogMaxFolderEntries + 1;
             ++index)
        {
            const auto suffix = std::to_string(index);
            input.folders.push_back({
                folderId("folder-" + suffix),
                DocumentCatalogPath("folder-path-" + suffix),
                DocumentFolderKey("folder-key-" + suffix),
                "Folder " + suffix,
                DocumentFolderOrder(static_cast<std::int32_t>(index))
            });
        }
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }
}

void NextApplicationDocumentCatalogTests::folderPathsMustBeUniqueAndHierarchyMustBeRooted()
{
    {
        auto input = validInput();
        auto duplicatePath = input.folders.front();
        duplicatePath.id = folderId("folder-guides-alias");
        input.folders.push_back(std::move(duplicatePath));
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().parentPath = DocumentCatalogPath("Missing");
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().parentPath = DocumentCatalogPath("Section");
        input.folders.push_back({
            folderId("folder-section"),
            DocumentCatalogPath("Section"),
            DocumentFolderKey("section"),
            "Section",
            DocumentFolderOrder(11)
        });
        input.folders.back().parentPath = DocumentCatalogPath("Guides");
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }
}

void NextApplicationDocumentCatalogTests::validNestedFoldersArePreserved()
{
    auto input = validInput();
    auto root = input.folders.front();
    root.id = folderId("folder-catalog");
    root.path = DocumentCatalogPath("Catalog");
    root.key = DocumentFolderKey("catalog");
    root.displayName = "Catalog";

    auto child = input.folders.front();
    child.path = DocumentCatalogPath("Catalog/Guides");
    child.parentPath = root.path;

    auto grandchild = input.folders.front();
    grandchild.id = folderId("folder-unit-1");
    grandchild.path = DocumentCatalogPath("Catalog/Guides/Unit 1");
    grandchild.parentPath = child.path;
    grandchild.key = DocumentFolderKey("unit-1");
    grandchild.displayName = "Unit 1";

    // The child and its parent need not appear in hierarchy order.
    input.folders = {child, grandchild, root};
    input.documents.front().folderId = child.id;
    input.documents.front().path = DocumentCatalogPath(
        "Catalog/Guides/lesson-planning.pdf"
        );

    const auto result = DocumentCatalogProjection::create(std::move(input));
    QVERIFY(result);
    QCOMPARE(result.value().folders().size(), std::size_t(3));
    QCOMPARE(result.value().folders()[0].path.value(), std::string("Catalog/Guides"));
    QCOMPARE(result.value().folders()[0].parentPath.value(), std::string("Catalog"));
    QCOMPARE(
        result.value().folders()[1].path.value(),
        std::string("Catalog/Guides/Unit 1")
        );
    QCOMPARE(
        result.value().folders()[1].parentPath.value(),
        std::string("Catalog/Guides")
        );
    QCOMPARE(result.value().folders()[2].path.value(), std::string("Catalog"));
    QVERIFY(result.value().folders()[2].parentPath.value().empty());
    QCOMPARE(
        result.value().documents().front().folderId.value(),
        std::string("folder-guides")
        );
}

void NextApplicationDocumentCatalogTests::unicodeWhitespaceIsRejectedAcrossBlankValidatedFields()
{
    enum class Field
    {
        FolderId,
        FolderPath,
        FolderParentPath,
        FolderKey,
        FolderDisplayName,
        DocumentId,
        DocumentFolderId,
        DocumentPath,
        DocumentKey,
        DocumentDisplayName,
        DocumentContentReference,
        DocumentExportReference
    };

    struct InvalidFieldCase final
    {
        Field field;
        const char* expectedMessage;
    };

    const InvalidFieldCase cases[] = {
        {
            Field::FolderId,
            "Document folder identifier must be non-blank and bounded."
        },
        {
            Field::FolderPath,
            "Document folder path, parent path, key, and display name must be bounded; parent path may be empty for a root folder."
        },
        {
            Field::FolderParentPath,
            "Document folder path, parent path, key, and display name must be bounded; parent path may be empty for a root folder."
        },
        {
            Field::FolderKey,
            "Document folder path, parent path, key, and display name must be bounded; parent path may be empty for a root folder."
        },
        {
            Field::FolderDisplayName,
            "Document folder path, parent path, key, and display name must be bounded; parent path may be empty for a root folder."
        },
        {
            Field::DocumentId,
            "Document identifier must be non-blank and bounded."
        },
        {
            Field::DocumentFolderId,
            "Document folder identifier must be non-blank and bounded."
        },
        {
            Field::DocumentPath,
            "Document path, key, and display name must be bounded and non-blank."
        },
        {
            Field::DocumentKey,
            "Document path, key, and display name must be bounded and non-blank."
        },
        {
            Field::DocumentDisplayName,
            "Document path, key, and display name must be bounded and non-blank."
        },
        {
            Field::DocumentContentReference,
            "Document content reference must be non-blank and bounded."
        },
        {
            Field::DocumentExportReference,
            "Document export reference must be non-blank and bounded."
        }
    };

    const std::string unicodeWhitespaceValues[] = {
        std::string("\xC2\xA0", 2),
        std::string("\xE3\x80\x80", 3)
    };

    for (const auto& whitespace : unicodeWhitespaceValues)
    {
        for (const auto& testCase : cases)
        {
            auto input = validInput();
            switch (testCase.field)
            {
                case Field::FolderId:
                    input.folders.front().id = folderId(whitespace);
                    break;
                case Field::FolderPath:
                    input.folders.front().path = DocumentCatalogPath(whitespace);
                    break;
                case Field::FolderParentPath:
                    input.folders.front().parentPath = DocumentCatalogPath(whitespace);
                    break;
                case Field::FolderKey:
                    input.folders.front().key = DocumentFolderKey(whitespace);
                    break;
                case Field::FolderDisplayName:
                    input.folders.front().displayName = whitespace;
                    break;
                case Field::DocumentId:
                    input.documents.front().id = documentId(whitespace);
                    break;
                case Field::DocumentFolderId:
                    input.documents.front().folderId = folderId(whitespace);
                    break;
                case Field::DocumentPath:
                    input.documents.front().path = DocumentCatalogPath(whitespace);
                    break;
                case Field::DocumentKey:
                    input.documents.front().key = DocumentEntryKey(whitespace);
                    break;
                case Field::DocumentDisplayName:
                    input.documents.front().displayName = whitespace;
                    break;
                case Field::DocumentContentReference:
                    input.documents.front().contentReference =
                        DocumentContentReference(whitespace);
                    break;
                case Field::DocumentExportReference:
                    input.documents.front().exportable = true;
                    input.documents.front().exportReference =
                        DocumentContentReference(whitespace);
                    break;
            }

            verifyInvalidWithMessage(
                DocumentCatalogProjection::create(std::move(input)),
                testCase.expectedMessage
                );
        }
    }

    // The first failing document field remains observable when later fields
    // are invalid too, preserving the established validation order.
    auto input = validInput();
    input.documents.front().id = documentId(unicodeWhitespaceValues[0]);
    input.documents.front().path = DocumentCatalogPath(unicodeWhitespaceValues[1]);
    input.documents.front().contentReference = DocumentContentReference(
        unicodeWhitespaceValues[1]
        );
    verifyInvalidWithMessage(
        DocumentCatalogProjection::create(std::move(input)),
        "Document identifier must be non-blank and bounded."
        );
}

void NextApplicationDocumentCatalogTests::nonBlankUnicodeAndMalformedUtf8ArePreserved()
{
    const std::string embeddedValues[] = {
        std::string("\xC2\xA0", 2),
        std::string("\xE3\x80\x80", 3),
        std::string(" "),
        std::string("\xFF", 1)
    };

    for (const auto& embeddedValue : embeddedValues)
    {
        auto input = validInput();
        const std::string folderIdValue = "folder-" + embeddedValue;
        const std::string documentIdValue = "document-" + embeddedValue;
        const std::string folderPath = "目录" + embeddedValue + "资料";
        const std::string folderParentPath = "parent" + embeddedValue;
        const std::string folderKey = "course" + embeddedValue + "key";
        const std::string folderDisplayName = "课程" + embeddedValue + "资料";
        const std::string documentPath = "目录/" + embeddedValue + "lesson.pdf";
        const std::string documentKey = "lesson" + embeddedValue + "plan";
        const std::string documentDisplayName = "课程" + embeddedValue + "Lesson";
        const std::string contentReference =
            "resource://documents/" + embeddedValue + "lesson.pdf";

        input.folders.front().id = folderId(folderIdValue);
        input.folders.front().path = DocumentCatalogPath(folderPath);
        input.folders.front().parentPath = DocumentCatalogPath(folderParentPath);
        input.folders.front().key = DocumentFolderKey(folderKey);
        input.folders.front().displayName = folderDisplayName;
        input.documents.front().id = documentId(documentIdValue);
        input.documents.front().folderId = input.folders.front().id;
        input.documents.front().path = DocumentCatalogPath(documentPath);
        input.documents.front().key = DocumentEntryKey(documentKey);
        input.documents.front().displayName = documentDisplayName;
        input.documents.front().contentReference =
            DocumentContentReference(contentReference);

        auto parent = input.folders.front();
        parent.id = folderId("folder-parent-" + embeddedValue);
        parent.path = DocumentCatalogPath(folderParentPath);
        parent.parentPath = DocumentCatalogPath();
        parent.key = DocumentFolderKey("parent-key");
        parent.displayName = "Parent Folder";
        input.folders.push_back(std::move(parent));

        const auto result = DocumentCatalogProjection::create(std::move(input));
        QVERIFY(result);
        const auto& folder = result.value().folders().front();
        QCOMPARE(folder.id.value(), folderIdValue);
        QCOMPARE(folder.path.value(), folderPath);
        QCOMPARE(folder.parentPath.value(), folderParentPath);
        QCOMPARE(folder.key.value(), folderKey);
        QCOMPARE(folder.displayName, folderDisplayName);

        const auto& document = result.value().documents().front();
        QCOMPARE(document.id.value(), documentIdValue);
        QCOMPARE(document.folderId.value(), folderIdValue);
        QCOMPARE(document.path.value(), documentPath);
        QCOMPARE(document.key.value(), documentKey);
        QCOMPARE(document.displayName, documentDisplayName);
        QCOMPARE(document.contentReference.value(), contentReference);
        QVERIFY(!document.exportReference.has_value());
    }
}

void NextApplicationDocumentCatalogTests::documentCollectionOverflowIsRejected()
{
    auto input = validInput();
    input.documents.clear();
    input.documents.reserve(kDocumentCatalogMaxDocumentEntries + 1);
    for (std::size_t index = 0;
         index < kDocumentCatalogMaxDocumentEntries + 1;
         ++index)
    {
        const auto suffix = std::to_string(index);
        input.documents.push_back({
            documentId("document-" + suffix),
            input.folders.front().id,
            DocumentCatalogPath("Guides/document-" + suffix + ".pdf"),
            DocumentEntryKey("document-" + suffix),
            "Document " + suffix,
            DocumentOrder(static_cast<std::int32_t>(index)),
            index % 2 == 0,
            false,
            DocumentContentReference(
                "resource://documents/document-" + suffix + ".pdf"
                ),
            std::nullopt
        });
    }

    verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
}

void NextApplicationDocumentCatalogTests::negativeOrdersAreRejected()
{
    {
        auto input = validInput();
        input.folders.front().order = DocumentFolderOrder(-1);
        verifyInvalidWithMessage(
            DocumentCatalogProjection::create(std::move(input)),
            "Document folder order must not be negative."
            );
    }

    {
        auto input = validInput();
        input.documents.front().order = DocumentOrder(-1);
        verifyInvalidWithMessage(
            DocumentCatalogProjection::create(std::move(input)),
            "Document order must not be negative."
            );
    }
}

void NextApplicationDocumentCatalogTests::optionalExportReferencePreservesAbsenceAndPresence()
{
    const auto withoutExport = DocumentCatalogProjection::create(validInput());
    QVERIFY(withoutExport);
    const auto& absent = withoutExport.value().documents().front();
    QVERIFY(!absent.exportReference.has_value());
    QVERIFY(!absent.exportContentReference().has_value());

    auto input = validInput();
    auto& document = input.documents.front();
    document.exportable = true;
    document.exportReference = DocumentContentReference(
        "resource://documents/lesson-planning.pptx"
        );

    const auto withExport = DocumentCatalogProjection::create(std::move(input));
    QVERIFY(withExport);
    const auto& present = withExport.value().documents().front();
    QVERIFY(present.exportable);
    QVERIFY(present.exportReference.has_value());
    QVERIFY(present.exportContentReference().has_value());
    QCOMPARE(
        present.exportReference->value(),
        std::string("resource://documents/lesson-planning.pptx")
        );

    auto inconsistent = validInput();
    inconsistent.documents.front().exportReference = DocumentContentReference(
        "resource://documents/unused-export.pptx"
        );
    verifyInvalid(DocumentCatalogProjection::create(std::move(inconsistent)));
}

void NextApplicationDocumentCatalogTests::exportReferenceBoundsAreRejected()
{
    for (const std::string& reference : {
             std::string(" \t\r\n"),
             std::string(kDocumentCatalogMaxReferenceLength + 1, 'e')
         })
    {
        auto input = validInput();
        input.documents.front().exportable = true;
        input.documents.front().exportReference =
            DocumentContentReference(reference);
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }
}

void NextApplicationDocumentCatalogTests::projectionsAreCopyableEqualAndIndependentlyReleasable()
{
    static_assert(std::is_copy_constructible_v<DocumentFolderMetadata>);
    static_assert(std::is_copy_assignable_v<DocumentFolderMetadata>);
    static_assert(std::is_copy_constructible_v<DocumentEntryMetadata>);
    static_assert(std::is_copy_assignable_v<DocumentEntryMetadata>);
    static_assert(std::is_copy_constructible_v<DocumentCatalogProjection>);
    static_assert(std::is_copy_assignable_v<DocumentCatalogProjection>);

    const auto result = DocumentCatalogProjection::create(validInput());
    QVERIFY(result);
    DocumentCatalogProjection original = result.value();
    const DocumentCatalogProjection copy = original;
    QVERIFY(copy == original);

    DocumentCatalogProjection released = std::move(original);
    QVERIFY(released == copy);

    original = DocumentCatalogProjection{};
    QVERIFY(original.empty());
    QVERIFY(released == copy);
}

void NextApplicationDocumentCatalogTests::contractHasOnlyAdapterNeutralMetadata()
{
    static_assert(std::is_same_v<DocumentContentReference::Text, std::string>);
    static_assert(std::is_same_v<
        decltype(std::declval<const DocumentCatalogProjection>().folders()),
        const std::vector<DocumentFolderMetadata>&
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const DocumentCatalogProjection>().documents()),
        const std::vector<DocumentEntryMetadata>&
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<const DocumentCatalogProjection>().folders())
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<const DocumentCatalogProjection>().documents())
        >);

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextApplicationDocumentCatalogTests)

#include "next_application_document_catalog_tests.moc"
