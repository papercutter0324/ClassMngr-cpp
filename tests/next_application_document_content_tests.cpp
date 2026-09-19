#include "next/application/document_content_session.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

class NextApplicationDocumentContentTests final : public QObject
{
    Q_OBJECT

private slots:
    void initialStateIsIdle();
    void requestLoadingAndReadyLifecycleIsDeterministic();
    void failurePreservesStructuredErrorAndReferenceMetadata();
    void invalidReferencesDoNotMutateState();
    void invalidTransitionsDoNotMutateState();
    void releaseClearsMetadataAndAllowsRerequest();
    void lateEventsAfterTerminalStatesDoNotMutateSnapshot();
    void staleEventsFromPriorRequestAreRejectedAfterRerequest();
    void snapshotsAndSessionsAreCopyableEqualValues();
    void applicationContractHasNoContentObjectOrBytes();
};

void NextApplicationDocumentContentTests::initialStateIsIdle()
{
    DocumentContentSession session;

    const DocumentContentSnapshot snapshot = session.snapshot();

    QCOMPARE(snapshot.phase(), DocumentContentPhase::Idle);
    QVERIFY(!snapshot.reference().has_value());
    QVERIFY(!snapshot.error().has_value());
}

void NextApplicationDocumentContentTests::requestLoadingAndReadyLifecycleIsDeterministic()
{
    DocumentContentSession session;
    const DocumentContentReference reference("resource://guide.pdf");

    const auto request = session.request(reference);
    QVERIFY(request);
    const auto token = request.value();
    const DocumentContentSnapshot requested = session.snapshot();
    QCOMPARE(requested.phase(), DocumentContentPhase::Requested);
    QVERIFY(requested.reference().has_value());
    QCOMPARE(requested.reference()->value(), reference.value());
    QVERIFY(!requested.error().has_value());

    const auto replacement = session.request(
        DocumentContentReference("resource://replacement.pdf")
        );
    QVERIFY(!replacement);
    QCOMPARE(replacement.error().code, ErrorCode::Conflict);
    QVERIFY(session.snapshot() == requested);

    QVERIFY(session.beginLoading(token));
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Loading);
    QVERIFY(session.markReady(token));

    const DocumentContentSnapshot ready = session.snapshot();
    QCOMPARE(ready.phase(), DocumentContentPhase::Ready);
    QVERIFY(ready.reference().has_value());
    QCOMPARE(ready.reference()->value(), reference.value());
    QVERIFY(!ready.error().has_value());
}

void NextApplicationDocumentContentTests::failurePreservesStructuredErrorAndReferenceMetadata()
{
    DocumentContentSession session;
    const DocumentContentReference reference("document-token-1");
    const OperationError expected{
        .code = ErrorCode::Technical,
        .message = "The document adapter failed to load content.",
        .recoverable = false
    };

    const auto request = session.request(reference);
    QVERIFY(request);
    const auto token = request.value();
    QVERIFY(session.beginLoading(token));
    QVERIFY(session.fail(token, expected));

    const DocumentContentSnapshot failed = session.snapshot();
    QCOMPARE(failed.phase(), DocumentContentPhase::Failed);
    QVERIFY(failed.reference().has_value());
    QCOMPARE(failed.reference()->value(), reference.value());
    QVERIFY(failed.error().has_value());
    QVERIFY(*failed.error() == expected);

    const auto lateReady = session.markReady(token);
    QVERIFY(!lateReady);
    QCOMPARE(lateReady.error().code, ErrorCode::Conflict);
    QVERIFY(session.snapshot() == failed);

    // Failed retains no active content object, so a new request resets the
    // terminal error without requiring stale failure metadata to be reused.
    const auto restartedRequest = session.request(
        DocumentContentReference("document-token-2")
        );
    QVERIFY(restartedRequest);
    const DocumentContentSnapshot restarted = session.snapshot();
    QCOMPARE(restarted.phase(), DocumentContentPhase::Requested);
    QVERIFY(restarted.reference().has_value());
    QCOMPARE(restarted.reference()->value(), std::string("document-token-2"));
    QVERIFY(!restarted.error().has_value());
}

void NextApplicationDocumentContentTests::invalidReferencesDoNotMutateState()
{
    DocumentContentSession session;
    const DocumentContentSnapshot initial = session.snapshot();

    const auto empty = session.request(DocumentContentReference{});
    QVERIFY(!empty);
    QCOMPARE(empty.error().code, ErrorCode::InvalidInput);
    QVERIFY(session.snapshot() == initial);

    const auto whitespace = session.request(
        DocumentContentReference(" \t\r\n")
        );
    QVERIFY(!whitespace);
    QCOMPARE(whitespace.error().code, ErrorCode::InvalidInput);
    QVERIFY(session.snapshot() == initial);

    const std::string oversized(
        kDocumentContentMaxReferenceLength + 1,
        'x'
        );
    const auto tooLong = session.request(
        DocumentContentReference(oversized)
        );
    QVERIFY(!tooLong);
    QCOMPARE(tooLong.error().code, ErrorCode::InvalidInput);
    QVERIFY(session.snapshot() == initial);

    const std::string atLimit(
        kDocumentContentMaxReferenceLength,
        'x'
        );
    QVERIFY(session.request(DocumentContentReference(atLimit)));
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Requested);
    QCOMPARE(session.snapshot().reference()->value(), atLimit);
}

void NextApplicationDocumentContentTests::invalidTransitionsDoNotMutateState()
{
    DocumentContentSession session;

    const auto idleRelease = session.release();

    QVERIFY(!idleRelease);
    QCOMPARE(idleRelease.error().code, ErrorCode::Conflict);
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Idle);

    const auto request = session.request(DocumentContentReference("guide.pdf"));
    QVERIFY(request);
    const auto token = request.value();
    const DocumentContentSnapshot requested = session.snapshot();
    const auto earlyReady = session.markReady(token);
    QVERIFY(!earlyReady);
    QCOMPARE(earlyReady.error().code, ErrorCode::Conflict);
    QVERIFY(session.snapshot() == requested);

    QVERIFY(session.beginLoading(token));
    const DocumentContentSnapshot loading = session.snapshot();
    const auto duplicateLoading = session.beginLoading(token);
    QVERIFY(!duplicateLoading);
    QCOMPARE(duplicateLoading.error().code, ErrorCode::Conflict);
    QVERIFY(session.snapshot() == loading);
}

void NextApplicationDocumentContentTests::releaseClearsMetadataAndAllowsRerequest()
{
    DocumentContentSession session;
    const auto request = session.request(DocumentContentReference("guide.pdf"));
    QVERIFY(request);
    const auto token = request.value();
    QVERIFY(session.beginLoading(token));
    QVERIFY(session.markReady(token));

    QVERIFY(session.release());
    const DocumentContentSnapshot released = session.snapshot();
    QCOMPARE(released.phase(), DocumentContentPhase::Released);
    QVERIFY(!released.reference().has_value());
    QVERIFY(!released.error().has_value());

    const auto lateLoading = session.beginLoading(token);
    const auto lateReady = session.markReady(token);
    const auto lateFailure = session.fail(
        token,
        OperationError{
            .code = ErrorCode::Technical,
            .message = "Late failure.",
            .recoverable = false
        }
        );
    const auto duplicateRelease = session.release();
    QVERIFY(!lateLoading);
    QVERIFY(!lateReady);
    QVERIFY(!lateFailure);
    QVERIFY(!duplicateRelease);
    QCOMPARE(lateLoading.error().code, ErrorCode::Conflict);
    QCOMPARE(lateReady.error().code, ErrorCode::Conflict);
    QCOMPARE(lateFailure.error().code, ErrorCode::Conflict);
    QCOMPARE(duplicateRelease.error().code, ErrorCode::Conflict);
    QVERIFY(session.snapshot() == released);

    QVERIFY(session.request(DocumentContentReference("reopened://guide")));
    const DocumentContentSnapshot requested = session.snapshot();
    QCOMPARE(requested.phase(), DocumentContentPhase::Requested);
    QVERIFY(requested.reference().has_value());
    QCOMPARE(requested.reference()->value(), std::string("reopened://guide"));
    QVERIFY(!requested.error().has_value());
}

void NextApplicationDocumentContentTests::lateEventsAfterTerminalStatesDoNotMutateSnapshot()
{
    DocumentContentSession failedSession;
    const auto request = failedSession.request(
        DocumentContentReference("failed.pdf")
        );
    QVERIFY(request);
    const auto token = request.value();
    const OperationError expected{
        .code = ErrorCode::Validation,
        .message = "The document reference was rejected by the adapter.",
        .recoverable = true
    };
    QVERIFY(failedSession.fail(token, expected));

    const DocumentContentSnapshot failed = failedSession.snapshot();
    const auto lateLoading = failedSession.beginLoading(token);
    const auto lateReady = failedSession.markReady(token);
    const auto lateFailure = failedSession.fail(
        token,
        OperationError{
            .code = ErrorCode::Technical,
            .message = "A later failure.",
            .recoverable = false
        }
        );
    QVERIFY(!lateLoading);
    QVERIFY(!lateReady);
    QVERIFY(!lateFailure);
    QCOMPARE(lateLoading.error().code, ErrorCode::Conflict);
    QCOMPARE(lateReady.error().code, ErrorCode::Conflict);
    QCOMPARE(lateFailure.error().code, ErrorCode::Conflict);
    QVERIFY(failedSession.snapshot() == failed);

    DocumentContentSession releasedSession;
    const auto releasedRequest = releasedSession.request(
        DocumentContentReference("released.pdf")
        );
    QVERIFY(releasedRequest);
    QVERIFY(releasedSession.release());
    const DocumentContentSnapshot released = releasedSession.snapshot();
    const auto lateRequest = releasedSession.request(
        DocumentContentReference("replacement.pdf")
        );
    QVERIFY(lateRequest);
    QCOMPARE(releasedSession.snapshot().phase(), DocumentContentPhase::Requested);
    QVERIFY(releasedSession.snapshot().reference().has_value());
    QVERIFY(releasedSession.snapshot().reference()->value() == "replacement.pdf");
    QVERIFY(releasedSession.snapshot().error() == std::nullopt);
    QVERIFY(released != releasedSession.snapshot());
}

void NextApplicationDocumentContentTests::staleEventsFromPriorRequestAreRejectedAfterRerequest()
{
    DocumentContentSession session;
    const OperationError firstFailure{
        .code = ErrorCode::Technical,
        .message = "The first document request failed.",
        .recoverable = true
    };

    const auto firstRequest = session.request(
        DocumentContentReference("first.pdf")
        );
    QVERIFY(firstRequest);
    const auto firstToken = firstRequest.value();
    QVERIFY(session.beginLoading(firstToken));
    QVERIFY(session.fail(firstToken, firstFailure));

    const auto secondRequest = session.request(
        DocumentContentReference("second.pdf")
        );
    QVERIFY(secondRequest);
    const auto secondToken = secondRequest.value();
    QVERIFY(!(secondToken == firstToken));

    const auto staleLoading = session.beginLoading(firstToken);
    const auto staleReady = session.markReady(firstToken);
    const auto staleFailure = session.fail(firstToken, firstFailure);
    QVERIFY(!staleLoading);
    QVERIFY(!staleReady);
    QVERIFY(!staleFailure);
    QCOMPARE(staleLoading.error().code, ErrorCode::Conflict);
    QCOMPARE(staleReady.error().code, ErrorCode::Conflict);
    QCOMPARE(staleFailure.error().code, ErrorCode::Conflict);
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Requested);
    QCOMPARE(
        session.snapshot().reference()->value(),
        std::string("second.pdf")
        );

    QVERIFY(session.beginLoading(secondToken));
    QVERIFY(session.markReady(secondToken));

    QVERIFY(session.release());
    const auto thirdRequest = session.request(
        DocumentContentReference("third.pdf")
        );
    QVERIFY(thirdRequest);
    const auto thirdToken = thirdRequest.value();
    QVERIFY(!(thirdToken == secondToken));

    const auto staleAfterRelease = session.beginLoading(secondToken);
    QVERIFY(!staleAfterRelease);
    QCOMPARE(staleAfterRelease.error().code, ErrorCode::Conflict);
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Requested);

    QVERIFY(session.beginLoading(thirdToken));
    QVERIFY(session.markReady(thirdToken));
    QCOMPARE(session.snapshot().phase(), DocumentContentPhase::Ready);
}

void NextApplicationDocumentContentTests::snapshotsAndSessionsAreCopyableEqualValues()
{
    static_assert(std::is_copy_constructible_v<DocumentContentReference>);
    static_assert(std::is_copy_assignable_v<DocumentContentReference>);
    static_assert(std::is_copy_constructible_v<DocumentContentSnapshot>);
    static_assert(std::is_copy_assignable_v<DocumentContentSnapshot>);
    static_assert(std::is_copy_constructible_v<DocumentContentSession>);
    static_assert(std::is_copy_assignable_v<DocumentContentSession>);

    DocumentContentSession session;
    const auto request = session.request(DocumentContentReference("guide.pdf"));
    QVERIFY(request);
    const auto token = request.value();
    QVERIFY(session.beginLoading(token));

    const DocumentContentSnapshot originalSnapshot = session.snapshot();
    const DocumentContentSnapshot copiedSnapshot = originalSnapshot;
    QVERIFY(copiedSnapshot == originalSnapshot);

    const DocumentContentSession copiedSession = session;
    QVERIFY(copiedSession == session);
    QVERIFY(session.markReady(token));
    QVERIFY(copiedSession.snapshot() == originalSnapshot);
    QVERIFY(originalSnapshot == copiedSnapshot);

    DocumentContentSession assignedSession;
    assignedSession = session;
    QVERIFY(assignedSession == session);
}

void NextApplicationDocumentContentTests::applicationContractHasNoContentObjectOrBytes()
{
    // This test intentionally uses only the adapter-neutral metadata surface.
    // The header has no Qt include, viewer pointer, document object, or byte
    // container; actual content is owned and released by the adapter.
    static_assert(std::is_same_v<DocumentContentReference::Text, std::string>);
    static_assert(std::is_same_v<
        decltype(std::declval<DocumentContentSnapshot>().reference()),
        const std::optional<DocumentContentReference>&
        >);
    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextApplicationDocumentContentTests)

#include "next_application_document_content_tests.moc"
