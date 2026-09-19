#include "next/application/import_job_state.h"

#include <QtTest/QtTest>

#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

class NextApplicationImportJobTests final : public QObject
{
    Q_OBJECT

private slots:
    void initialStateIsIdle();
    void startAndProgressAreDeterministic();
    void invalidProgressDoesNotMutateState();
    void duplicateStartReturnsRecoverableConflict();
    void cancellationRequestIsIdempotent();
    void acknowledgementTransitionsToCanceled();
    void directCancelTransitionsToCanceled();
    void completeRequiresAllProgressAndSucceeds();
    void zeroItemJobCompletesWithoutProgress();
    void failurePreservesStructuredError();
    void terminalStateIsImmutableUntilRestart();
    void canceledStateRejectsLateEventsAndRestarts();
    void failedStateRejectsLateEventsAndRestarts();
    void snapshotsAndOwnersAreCopyableValuesWithEquality();
};

void NextApplicationImportJobTests::initialStateIsIdle()
{
    ImportJobState state;

    const ImportJobSnapshot snapshot = state.snapshot();
    const ImportJobSnapshot::ItemCount zero = 0;

    QCOMPARE(snapshot.phase(), ImportJobPhase::Idle);
    QCOMPARE(snapshot.totalItems(), zero);
    QCOMPARE(snapshot.completedItems(), zero);
    QVERIFY(!snapshot.cancellationRequested());
    QVERIFY(!snapshot.error().has_value());
}

void NextApplicationImportJobTests::startAndProgressAreDeterministic()
{
    ImportJobState state;

    QVERIFY(state.start(3));
    const ImportJobSnapshot started = state.snapshot();
    QCOMPARE(started.phase(), ImportJobPhase::Running);
    QCOMPARE(started.totalItems(), ImportJobSnapshot::ItemCount{3});
    QCOMPARE(started.completedItems(), ImportJobSnapshot::ItemCount{0});
    QVERIFY(!started.cancellationRequested());
    QVERIFY(!started.error().has_value());

    QVERIFY(state.reportProgress(1));
    const ImportJobSnapshot progressed = state.snapshot();
    QCOMPARE(progressed.phase(), ImportJobPhase::Running);
    QCOMPARE(progressed.totalItems(), ImportJobSnapshot::ItemCount{3});
    QCOMPARE(progressed.completedItems(), ImportJobSnapshot::ItemCount{1});
}

void NextApplicationImportJobTests::invalidProgressDoesNotMutateState()
{
    ImportJobState state;
    QVERIFY(state.start(2));
    QVERIFY(state.reportProgress(1));
    const ImportJobSnapshot before = state.snapshot();

    const auto result = state.reportProgress(3);

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(state.snapshot() == before);
}

void NextApplicationImportJobTests::duplicateStartReturnsRecoverableConflict()
{
    ImportJobState state;
    QVERIFY(state.start(2));
    const ImportJobSnapshot before = state.snapshot();

    const auto result = state.start(4);

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QVERIFY(result.error().recoverable);
    QVERIFY(state.snapshot() == before);
}

void NextApplicationImportJobTests::cancellationRequestIsIdempotent()
{
    ImportJobState state;
    QVERIFY(state.start(3));

    QVERIFY(state.requestCancellation());
    const ImportJobSnapshot requested = state.snapshot();
    QVERIFY(requested.cancellationRequested());
    QCOMPARE(requested.phase(), ImportJobPhase::Running);

    QVERIFY(state.requestCancellation());
    QCOMPARE(state.snapshot(), requested);
}

void NextApplicationImportJobTests::acknowledgementTransitionsToCanceled()
{
    ImportJobState state;
    QVERIFY(state.start(4));
    QVERIFY(state.reportProgress(2));
    QVERIFY(state.requestCancellation());

    QVERIFY(state.acknowledgeCancellation());
    const ImportJobSnapshot canceled = state.snapshot();
    QCOMPARE(canceled.phase(), ImportJobPhase::Canceled);
    QCOMPARE(canceled.totalItems(), ImportJobSnapshot::ItemCount{4});
    QCOMPARE(canceled.completedItems(), ImportJobSnapshot::ItemCount{2});
    QVERIFY(canceled.cancellationRequested());
    QVERIFY(!canceled.error().has_value());

    const auto duplicate = state.acknowledgeCancellation();
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, ErrorCode::Conflict);
    QVERIFY(state.snapshot() == canceled);
}

void NextApplicationImportJobTests::directCancelTransitionsToCanceled()
{
    ImportJobState state;
    QVERIFY(state.start(1));

    QVERIFY(state.cancel());
    QCOMPARE(state.snapshot().phase(), ImportJobPhase::Canceled);
}

void NextApplicationImportJobTests::completeRequiresAllProgressAndSucceeds()
{
    ImportJobState state;
    QVERIFY(state.start(2));

    const auto incomplete = state.complete();
    QVERIFY(!incomplete);
    QCOMPARE(incomplete.error().code, ErrorCode::Conflict);
    QCOMPARE(state.snapshot().phase(), ImportJobPhase::Running);

    QVERIFY(state.reportProgress(2));
    QVERIFY(state.complete());
    const ImportJobSnapshot completed = state.snapshot();
    QCOMPARE(completed.phase(), ImportJobPhase::Completed);
    QCOMPARE(completed.completedItems(), ImportJobSnapshot::ItemCount{2});
}

void NextApplicationImportJobTests::zeroItemJobCompletesWithoutProgress()
{
    ImportJobState state;

    QVERIFY(state.start(0));
    const ImportJobSnapshot started = state.snapshot();
    QCOMPARE(started.phase(), ImportJobPhase::Running);
    QCOMPARE(started.totalItems(), ImportJobSnapshot::ItemCount{0});
    QCOMPARE(started.completedItems(), ImportJobSnapshot::ItemCount{0});

    QVERIFY(state.complete());
    const ImportJobSnapshot completed = state.snapshot();
    QCOMPARE(completed.phase(), ImportJobPhase::Completed);
    QCOMPARE(completed.totalItems(), ImportJobSnapshot::ItemCount{0});
    QCOMPARE(completed.completedItems(), ImportJobSnapshot::ItemCount{0});
    QVERIFY(!completed.cancellationRequested());
    QVERIFY(!completed.error().has_value());

    QVERIFY(state.start(1));
    const ImportJobSnapshot restarted = state.snapshot();
    QCOMPARE(restarted.phase(), ImportJobPhase::Running);
    QCOMPARE(restarted.totalItems(), ImportJobSnapshot::ItemCount{1});
    QCOMPARE(restarted.completedItems(), ImportJobSnapshot::ItemCount{0});
    QVERIFY(!restarted.cancellationRequested());
    QVERIFY(!restarted.error().has_value());
}

void NextApplicationImportJobTests::failurePreservesStructuredError()
{
    ImportJobState state;
    QVERIFY(state.start(3));
    QVERIFY(state.reportProgress(1));

    const OperationError expected{
        .code = ErrorCode::Technical,
        .message = "The import parser failed.",
        .recoverable = false
    };
    QVERIFY(state.fail(expected));

    const ImportJobSnapshot failed = state.snapshot();
    QCOMPARE(failed.phase(), ImportJobPhase::Failed);
    QCOMPARE(failed.completedItems(), ImportJobSnapshot::ItemCount{1});
    QVERIFY(failed.error().has_value());
    QVERIFY(*failed.error() == expected);
}

void NextApplicationImportJobTests::terminalStateIsImmutableUntilRestart()
{
    ImportJobState state;
    QVERIFY(state.start(1));
    QVERIFY(state.reportProgress(1));
    QVERIFY(state.complete());
    const ImportJobSnapshot terminal = state.snapshot();

    const auto progress = state.reportProgress(0);
    const auto request = state.requestCancellation();
    const auto acknowledge = state.acknowledgeCancellation();
    const auto complete = state.complete();
    const auto failure = state.fail(
        OperationError{
            .code = ErrorCode::Technical,
            .message = "A late failure.",
            .recoverable = false
        }
        );

    QVERIFY(!progress);
    QVERIFY(!request);
    QVERIFY(!acknowledge);
    QVERIFY(!complete);
    QVERIFY(!failure);
    QCOMPARE(progress.error().code, ErrorCode::Conflict);
    QCOMPARE(request.error().code, ErrorCode::Conflict);
    QCOMPARE(acknowledge.error().code, ErrorCode::Conflict);
    QCOMPARE(complete.error().code, ErrorCode::Conflict);
    QCOMPARE(failure.error().code, ErrorCode::Conflict);
    QVERIFY(state.snapshot() == terminal);

    QVERIFY(state.start(2));
    const ImportJobSnapshot restarted = state.snapshot();
    QCOMPARE(restarted.phase(), ImportJobPhase::Running);
    QCOMPARE(restarted.totalItems(), ImportJobSnapshot::ItemCount{2});
    QCOMPARE(restarted.completedItems(), ImportJobSnapshot::ItemCount{0});
    QVERIFY(!restarted.cancellationRequested());
    QVERIFY(!restarted.error().has_value());
}

void NextApplicationImportJobTests::canceledStateRejectsLateEventsAndRestarts()
{
    ImportJobState state;
    QVERIFY(state.start(3));
    QVERIFY(state.reportProgress(1));
    QVERIFY(state.requestCancellation());
    QVERIFY(state.acknowledgeCancellation());

    const ImportJobSnapshot canceled = state.snapshot();
    QCOMPARE(canceled.phase(), ImportJobPhase::Canceled);

    const auto progress = state.reportProgress(2);
    const auto request = state.requestCancellation();
    const auto complete = state.complete();
    const auto failure = state.fail(
        OperationError{
            .code = ErrorCode::Technical,
            .message = "A late canceled-job failure.",
            .recoverable = false
        }
        );

    QVERIFY(!progress);
    QVERIFY(!request);
    QVERIFY(!complete);
    QVERIFY(!failure);
    QCOMPARE(progress.error().code, ErrorCode::Conflict);
    QCOMPARE(request.error().code, ErrorCode::Conflict);
    QCOMPARE(complete.error().code, ErrorCode::Conflict);
    QCOMPARE(failure.error().code, ErrorCode::Conflict);
    QVERIFY(state.snapshot() == canceled);
    QVERIFY(!state.snapshot().error().has_value());

    QVERIFY(state.start(2));
    const ImportJobSnapshot restarted = state.snapshot();
    QCOMPARE(restarted.phase(), ImportJobPhase::Running);
    QCOMPARE(restarted.totalItems(), ImportJobSnapshot::ItemCount{2});
    QCOMPARE(restarted.completedItems(), ImportJobSnapshot::ItemCount{0});
    QVERIFY(!restarted.cancellationRequested());
    QVERIFY(!restarted.error().has_value());
}

void NextApplicationImportJobTests::failedStateRejectsLateEventsAndRestarts()
{
    ImportJobState state;
    QVERIFY(state.start(4));
    QVERIFY(state.reportProgress(2));

    const OperationError expected{
        .code = ErrorCode::Technical,
        .message = "The import failed during parsing.",
        .recoverable = false
    };
    QVERIFY(state.fail(expected));

    const ImportJobSnapshot failed = state.snapshot();
    QCOMPARE(failed.phase(), ImportJobPhase::Failed);
    QVERIFY(failed.error().has_value());
    QVERIFY(*failed.error() == expected);

    const auto progress = state.reportProgress(3);
    const auto request = state.requestCancellation();
    const auto complete = state.complete();
    const auto acknowledge = state.acknowledgeCancellation();

    QVERIFY(!progress);
    QVERIFY(!request);
    QVERIFY(!complete);
    QVERIFY(!acknowledge);
    QCOMPARE(progress.error().code, ErrorCode::Conflict);
    QCOMPARE(request.error().code, ErrorCode::Conflict);
    QCOMPARE(complete.error().code, ErrorCode::Conflict);
    QCOMPARE(acknowledge.error().code, ErrorCode::Conflict);
    QVERIFY(state.snapshot() == failed);
    QVERIFY(state.snapshot().error().has_value());
    QVERIFY(*state.snapshot().error() == expected);

    QVERIFY(state.start(1));
    const ImportJobSnapshot restarted = state.snapshot();
    QCOMPARE(restarted.phase(), ImportJobPhase::Running);
    QCOMPARE(restarted.totalItems(), ImportJobSnapshot::ItemCount{1});
    QCOMPARE(restarted.completedItems(), ImportJobSnapshot::ItemCount{0});
    QVERIFY(!restarted.cancellationRequested());
    QVERIFY(!restarted.error().has_value());
}

void NextApplicationImportJobTests::snapshotsAndOwnersAreCopyableValuesWithEquality()
{
    static_assert(std::is_copy_constructible_v<ImportJobSnapshot>);
    static_assert(std::is_copy_assignable_v<ImportJobSnapshot>);
    static_assert(std::is_copy_constructible_v<ImportJobState>);
    static_assert(std::is_copy_assignable_v<ImportJobState>);

    ImportJobState state;
    QVERIFY(state.start(2));
    QVERIFY(state.reportProgress(1));

    const ImportJobSnapshot original = state.snapshot();
    const ImportJobSnapshot copy = original;
    QVERIFY(copy == original);

    const ImportJobState stateCopy = state;
    QVERIFY(stateCopy.snapshot() == original);

    QVERIFY(state.reportProgress(2));
    QVERIFY(original == copy);
    QCOMPARE(original.completedItems(), ImportJobSnapshot::ItemCount{1});
    QCOMPARE(state.snapshot().completedItems(), ImportJobSnapshot::ItemCount{2});
}

QTEST_APPLESS_MAIN(NextApplicationImportJobTests)

#include "next_application_import_job_tests.moc"
