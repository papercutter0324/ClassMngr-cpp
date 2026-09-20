#include "next/application/document_catalog_projection.h"

#include <QtTest/QtTest>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

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
        "Guides",
        "guides",
        "Guides",
        10
    });
    input.documents.push_back({
        documentId("document-lesson-planning"),
        input.folders.front().id,
        "Guides/lesson-planning.pdf",
        "lesson-planning",
        "Lesson Planning",
        20,
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

}

class NextApplicationDocumentCatalogTests final : public QObject
{
    Q_OBJECT

private slots:
    void validConstructionPreservesMetadataAndOrdering();
    void completeMetadataIsPreserved();
    void typedFolderAndDocumentCategoriesRemainDistinct();
    void boundedValidationAndDuplicateIdsAreRejected();
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
    QCOMPARE(projection.folders().front().path, std::string("Guides"));
    QCOMPARE(projection.folders().front().parentPath, std::string());
    QCOMPARE(projection.folders().front().key, std::string("guides"));
    QCOMPARE(projection.folders().front().displayName, std::string("Guides"));
    QCOMPARE(projection.folders().front().order, std::int32_t(10));

    const auto& document = projection.documents().front();
    QCOMPARE(document.id.value(), std::string("document-lesson-planning"));
    QCOMPARE(document.folderId.value(), std::string("folder-guides"));
    QCOMPARE(document.path, std::string("Guides/lesson-planning.pdf"));
    QCOMPARE(document.key, std::string("lesson-planning"));
    QCOMPARE(document.displayName, std::string("Lesson Planning"));
    QCOMPARE(document.order, std::int32_t(20));
    QVERIFY(document.printable);
    QVERIFY(!document.exportable);
    QCOMPARE(
        document.contentReference.value(),
        std::string("resource://documents/lesson-planning.pdf")
        );
    QVERIFY(!document.exportReference.has_value());
    QVERIFY(projection.entries() == projection.documents());
}

void NextApplicationDocumentCatalogTests::completeMetadataIsPreserved()
{
    auto input = validInput();
    auto& folder = input.folders.front();
    folder.id = folderId("folder-curriculum");
    folder.path = "Catalog/Curriculum";
    folder.parentPath = "Catalog";
    folder.key = "curriculum";
    folder.displayName = "Curriculum";
    folder.order = 42;

    auto& document = input.documents.front();
    document.id = documentId("document-curriculum-guide");
    document.folderId = folder.id;
    document.path = "Catalog/Curriculum/guide.pdf";
    document.key = "curriculum-guide";
    document.displayName = "Curriculum Guide";
    document.order = 7;
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
    QCOMPARE(projectedFolder.path, std::string("Catalog/Curriculum"));
    QCOMPARE(projectedFolder.parentPath, std::string("Catalog"));
    QCOMPARE(projectedFolder.key, std::string("curriculum"));
    QCOMPARE(projectedFolder.displayName, std::string("Curriculum"));
    QCOMPARE(projectedFolder.order, std::int32_t(42));

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
        projectedDocument.path,
        std::string("Catalog/Curriculum/guide.pdf")
        );
    QCOMPARE(projectedDocument.key, std::string("curriculum-guide"));
    QCOMPARE(
        projectedDocument.displayName,
        std::string("Curriculum Guide")
        );
    QCOMPARE(projectedDocument.order, std::int32_t(7));
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
        input.folders.front().path = "\t\n";
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().parentPath = " \t\n";
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().parentPath = std::string(
            kDocumentCatalogMaxPathLength + 1,
            'p'
            );
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.folders.front().key = std::string(
            kDocumentCatalogMaxKeyLength + 1,
            'k'
            );
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
                "folder-path-" + suffix,
                "folder-key-" + suffix,
                "Folder " + suffix,
                static_cast<std::int32_t>(index)
            });
        }
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
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
            "Guides/document-" + suffix + ".pdf",
            "document-" + suffix,
            "Document " + suffix,
            static_cast<std::int32_t>(index),
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
        input.folders.front().order = -1;
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.documents.front().order = -1;
        verifyInvalid(DocumentCatalogProjection::create(std::move(input)));
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
