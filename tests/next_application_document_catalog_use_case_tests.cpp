#include "next/application/document_catalog_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

[[nodiscard]] Domain::DocumentId documentId(
    const char* value
    )
{
    return *Domain::DocumentId::fromString(value);
}

[[nodiscard]] Domain::DocumentFolderId folderId(
    const char* value
    )
{
    return *Domain::DocumentFolderId::fromString(value);
}

[[nodiscard]] DocumentCatalogProjection makeProjection(
    const bool withExport
    )
{
    DocumentCatalogProjectionInput input;
    input.folders.push_back({
        folderId("folder-guides"),
        "Guides",
        "guides",
        "Guides",
        0
    });
    input.documents.push_back({
        documentId("document-lesson"),
        input.folders.front().id,
        "Guides/lesson.pdf",
        "lesson",
        "Lesson",
        0,
        true,
        withExport,
        DocumentContentReference("resource://documents/lesson.pdf"),
        withExport
            ? std::optional<DocumentContentReference>(
                  DocumentContentReference("resource://exports/lesson.pptx")
                  )
            : std::nullopt
    });

    const auto result = DocumentCatalogProjection::create(std::move(input));
    Q_ASSERT(result);
    return result.value();
}

}

class NextApplicationDocumentCatalogUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void successfulLookupReturnsCopyableMetadata();
    void missingDocumentReturnsNotFoundWithoutMutatingSession();
    void primaryRequestReturnsReferenceAndSessionToken();
    void exportRequestReturnsOptionalReferenceAndSessionToken();
    void absentExportReturnsNotFoundWithoutMutatingSession();
    void activeSessionConflictPropagatesAndReleaseAllowsRerequest();
    void invalidReferenceErrorPropagatesUnchanged();
    void resultsAreCopyableAndTheBoundaryHasNoContentLeakage();
};

void NextApplicationDocumentCatalogUseCaseTests::successfulLookupReturnsCopyableMetadata()
{
    const DocumentCatalogProjection projection = makeProjection(true);
    DocumentContentSession session;
    const DocumentCatalogUseCase useCase(projection, session);

    const auto result = useCase.resolveDocument(documentId("document-lesson"));

    QVERIFY(result);
    QCOMPARE(result.value().id.value(), std::string("document-lesson"));
    QCOMPARE(result.value().folderId.value(), std::string("folder-guides"));
    QCOMPARE(result.value().path, std::string("Guides/lesson.pdf"));
    QCOMPARE(result.value().contentReference.value(),
        std::string("resource://documents/lesson.pdf"));
    QVERIFY(result.value().exportReference.has_value());
    QCOMPARE(result.value().exportReference->value(),
        std::string("resource://exports/lesson.pptx"));

    DocumentEntryMetadata copy = result.value();
    copy.displayName = "Changed copy";
    QVERIFY(copy != projection.documents().front());
    QCOMPARE(projection.documents().front().displayName,
        std::string("Lesson"));
}

void NextApplicationDocumentCatalogUseCaseTests::missingDocumentReturnsNotFoundWithoutMutatingSession()
{
    const DocumentCatalogProjection projection = makeProjection(true);
    DocumentContentSession session;
    const DocumentCatalogUseCase useCase(projection, session);
    const DocumentContentSnapshot before = session.snapshot();

    const auto lookup = useCase.resolveDocument(documentId("missing"));
    const auto request = useCase.requestPrimaryContent(documentId("missing"));

    QVERIFY(!lookup);
    QVERIFY(!request);
    QCOMPARE(lookup.error().code, ErrorCode::NotFound);
    QCOMPARE(request.error().code, ErrorCode::NotFound);
    QVERIFY(session.snapshot() == before);
}

void NextApplicationDocumentCatalogUseCaseTests::primaryRequestReturnsReferenceAndSessionToken()
{
    const DocumentCatalogProjection projection = makeProjection(true);
    DocumentContentSession session;
    const DocumentCatalogUseCase useCase(projection, session);

    const auto result = useCase.requestPrimaryContent(
        documentId("document-lesson")
        );

    QVERIFY(result);
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Requested);
    QVERIFY(session.snapshot().reference().has_value());
    QCOMPARE(session.snapshot().reference()->value(),
        std::string("resource://documents/lesson.pdf"));

    const DocumentCatalogUseCase::SessionToken copiedToken = result.value();
    QVERIFY(copiedToken == result.value());
}

void NextApplicationDocumentCatalogUseCaseTests::exportRequestReturnsOptionalReferenceAndSessionToken()
{
    const DocumentCatalogProjection projection = makeProjection(true);
    DocumentContentSession session;
    const DocumentCatalogUseCase useCase(projection, session);

    const auto result = useCase.requestExportContent(
        documentId("document-lesson")
        );

    QVERIFY(result);
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Requested);
    QVERIFY(session.snapshot().reference().has_value());
    QCOMPARE(session.snapshot().reference()->value(),
        std::string("resource://exports/lesson.pptx"));
    QVERIFY(result.value() == result.value());
}

void NextApplicationDocumentCatalogUseCaseTests::absentExportReturnsNotFoundWithoutMutatingSession()
{
    const DocumentCatalogProjection projection = makeProjection(false);
    DocumentContentSession session;
    const DocumentCatalogUseCase useCase(projection, session);
    const DocumentContentSnapshot before = session.snapshot();

    const auto result = useCase.requestExportContent(
        documentId("document-lesson")
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::NotFound);
    QVERIFY(!projection.documents().front().exportReference.has_value());
    QVERIFY(session.snapshot() == before);
}

void NextApplicationDocumentCatalogUseCaseTests::activeSessionConflictPropagatesAndReleaseAllowsRerequest()
{
    const DocumentCatalogProjection projection = makeProjection(true);
    DocumentContentSession session;
    const DocumentCatalogUseCase useCase(projection, session);

    const auto primary = useCase.requestPrimaryContent(
        documentId("document-lesson")
        );
    QVERIFY(primary);
    const DocumentContentSnapshot beforeConflict = session.snapshot();

    const auto expectedConflict = session.request(
        DocumentContentReference("resource://exports/lesson.pptx")
        );
    const auto actualConflict = useCase.requestExportContent(
        documentId("document-lesson")
        );

    QVERIFY(!expectedConflict);
    QVERIFY(!actualConflict);
    QVERIFY(actualConflict.error() == expectedConflict.error());
    QVERIFY(session.snapshot() == beforeConflict);
    QCOMPARE(actualConflict.error().code, ErrorCode::Conflict);

    QVERIFY(session.release());
    const auto exportRequest = useCase.requestExportContent(
        documentId("document-lesson")
        );
    QVERIFY(exportRequest);
    QVERIFY(!(exportRequest.value() == primary.value()));
    QCOMPARE(session.snapshot().reference()->value(),
        std::string("resource://exports/lesson.pptx"));
}

void NextApplicationDocumentCatalogUseCaseTests::invalidReferenceErrorPropagatesUnchanged()
{
    DocumentCatalogProjection projection = makeProjection(false);
    auto& entry = const_cast<DocumentEntryMetadata&>(
        projection.documents().front()
        );
    const std::string invalidReference(
        kDocumentContentMaxReferenceLength + 1,
        'x'
        );
    entry.contentReference = DocumentContentReference(invalidReference);

    DocumentContentSession session;
    const DocumentCatalogUseCase useCase(projection, session);
    DocumentContentSession expectedSession;
    const auto expected = expectedSession.request(
        DocumentContentReference(invalidReference)
        );
    const auto actual = useCase.requestPrimaryContent(
        documentId("document-lesson")
        );

    QVERIFY(!expected);
    QVERIFY(!actual);
    QVERIFY(actual.error() == expected.error());
    QCOMPARE(actual.error().code, ErrorCode::InvalidInput);
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Idle);
}

void NextApplicationDocumentCatalogUseCaseTests::resultsAreCopyableAndTheBoundaryHasNoContentLeakage()
{
    static_assert(std::is_copy_constructible_v<DocumentEntryMetadata>);
    static_assert(std::is_copy_assignable_v<DocumentEntryMetadata>);
    static_assert(std::is_copy_constructible_v<DocumentCatalogProjection>);
    static_assert(std::is_copy_constructible_v<DocumentContentSession>);
    static_assert(std::is_copy_constructible_v<
        Domain::Result<DocumentEntryMetadata>
        >);
    static_assert(std::is_copy_constructible_v<
        Domain::Result<DocumentCatalogUseCase::SessionToken>
        >);
    static_assert(std::is_same_v<DocumentContentReference::Text, std::string>);
    static_assert(!std::is_pointer_v<decltype(
        std::declval<const DocumentCatalogProjection&>().documents()
        )>);
    static_assert(std::is_same_v<
        decltype(std::declval<const DocumentCatalogUseCase&>().resolveDocument(
            std::declval<const Domain::DocumentId&>()
            )),
        Domain::Result<DocumentEntryMetadata>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<DocumentCatalogUseCase&>().requestPrimaryContent(
            std::declval<const Domain::DocumentId&>()
            )),
        Domain::Result<DocumentCatalogUseCase::SessionToken>
        >);

    const DocumentCatalogProjection projection = makeProjection(true);
    DocumentContentSession session;
    const DocumentCatalogUseCase useCase(projection, session);
    const auto result = useCase.resolveDocument(documentId("document-lesson"));
    QVERIFY(result);
    const auto copiedResult = result;
    QVERIFY(copiedResult.value() == result.value());
    QVERIFY(!session.snapshot().reference().has_value());
}

QTEST_APPLESS_MAIN(NextApplicationDocumentCatalogUseCaseTests)

#include "next_application_document_catalog_use_case_tests.moc"
