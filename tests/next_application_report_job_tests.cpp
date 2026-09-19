#include "next/application/report_job_state.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

class NextApplicationReportJobTests final : public QObject
{
    Q_OBJECT

private slots:
    void initialStateIsIdle();
    void startAndProgressAreDeterministic();
    void invalidProgressDoesNotMutateState();
    void duplicateStartReturnsRecoverableConflict();
    void cancellationRequestIsIdempotent();
    void acknowledgementTransitionsToCanceled();
    void completeRequiresAllProgressAndValidOutput();
    void invalidOutputReferencesDoNotMutateState();
    void zeroUnitJobCompletesWithOutputReference();
    void failurePreservesStructuredError();
    void failedStateRejectsLateEventsAndRestarts();
    void terminalStateIsImmutableUntilRestart();
    void canceledStateRejectsLateEventsAndRestarts();
    void snapshotsAndOwnersAreCopyableValuesWithEquality();
    void workerBoundaryUsesOwnerEventsAndBoundedSnapshot();
};

void NextApplicationReportJobTests::initialStateIsIdle()
{
    ReportJobState state;

    const ReportJobSnapshot snapshot = state.snapshot();
    const ReportJobSnapshot::UnitCount zero = 0;

    QCOMPARE(snapshot.phase(), ReportJobPhase::Idle);
    QCOMPARE(snapshot.totalUnits(), zero);
    QCOMPARE(snapshot.completedUnits(), zero);
    QVERIFY(!snapshot.cancellationRequested());
    QVERIFY(!snapshot.error().has_value());
    QVERIFY(!snapshot.outputReference().has_value());
}

void NextApplicationReportJobTests::startAndProgressAreDeterministic()
{
    ReportJobState state;

    QVERIFY(state.start(3));
    const ReportJobSnapshot started = state.snapshot();
    QCOMPARE(started.phase(), ReportJobPhase::Running);
    QCOMPARE(started.totalUnits(), ReportJobSnapshot::UnitCount{3});
    QCOMPARE(started.completedUnits(), ReportJobSnapshot::UnitCount{0});
    QVERIFY(!started.cancellationRequested());
    QVERIFY(!started.error().has_value());
    QVERIFY(!started.outputReference().has_value());

    QVERIFY(state.reportProgress(1));
    const ReportJobSnapshot progressed = state.snapshot();
    QCOMPARE(progressed.phase(), ReportJobPhase::Running);
    QCOMPARE(progressed.totalUnits(), ReportJobSnapshot::UnitCount{3});
    QCOMPARE(progressed.completedUnits(), ReportJobSnapshot::UnitCount{1});
}

void NextApplicationReportJobTests::invalidProgressDoesNotMutateState()
{
    ReportJobState state;
    QVERIFY(state.start(2));
    QVERIFY(state.reportProgress(1));
    const ReportJobSnapshot before = state.snapshot();

    const auto result = state.reportProgress(3);

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(state.snapshot() == before);
}

void NextApplicationReportJobTests::duplicateStartReturnsRecoverableConflict()
{
    ReportJobState state;
    QVERIFY(state.start(2));
    const ReportJobSnapshot before = state.snapshot();

    const auto result = state.start(4);

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QVERIFY(result.error().recoverable);
    QVERIFY(state.snapshot() == before);
}

void NextApplicationReportJobTests::cancellationRequestIsIdempotent()
{
    ReportJobState state;
    QVERIFY(state.start(3));

    QVERIFY(state.requestCancellation());
    const ReportJobSnapshot requested = state.snapshot();
    QVERIFY(requested.cancellationRequested());
    QCOMPARE(requested.phase(), ReportJobPhase::Running);

    QVERIFY(state.requestCancellation());
    QCOMPARE(state.snapshot(), requested);
}

void NextApplicationReportJobTests::acknowledgementTransitionsToCanceled()
{
    ReportJobState state;
    QVERIFY(state.start(4));
    QVERIFY(state.reportProgress(2));
    QVERIFY(state.requestCancellation());

    QVERIFY(state.acknowledgeCancellation());
    const ReportJobSnapshot canceled = state.snapshot();
    QCOMPARE(canceled.phase(), ReportJobPhase::Canceled);
    QCOMPARE(canceled.totalUnits(), ReportJobSnapshot::UnitCount{4});
    QCOMPARE(canceled.completedUnits(), ReportJobSnapshot::UnitCount{2});
    QVERIFY(canceled.cancellationRequested());
    QVERIFY(!canceled.error().has_value());
    QVERIFY(!canceled.outputReference().has_value());

    const auto duplicate = state.acknowledgeCancellation();
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, ErrorCode::Conflict);
    QVERIFY(state.snapshot() == canceled);
}

void NextApplicationReportJobTests::completeRequiresAllProgressAndValidOutput()
{
    ReportJobState state;
    QVERIFY(state.start(2));

    const auto invalidIncomplete = state.complete("");
    QVERIFY(!invalidIncomplete);
    QCOMPARE(invalidIncomplete.error().code, ErrorCode::InvalidInput);
    QCOMPARE(state.snapshot().phase(), ReportJobPhase::Running);

    const auto incomplete = state.complete("report.pdf");
    QVERIFY(!incomplete);
    QCOMPARE(incomplete.error().code, ErrorCode::Conflict);
    QCOMPARE(state.snapshot().phase(), ReportJobPhase::Running);
    QVERIFY(!state.snapshot().outputReference().has_value());

    QVERIFY(state.reportProgress(2));
    QVERIFY(state.complete("report.pdf"));
    const ReportJobSnapshot completed = state.snapshot();
    QCOMPARE(completed.phase(), ReportJobPhase::Completed);
    QCOMPARE(completed.completedUnits(), ReportJobSnapshot::UnitCount{2});
    QVERIFY(completed.outputReference().has_value());
    QCOMPARE(*completed.outputReference(), std::string("report.pdf"));
    QVERIFY(!completed.error().has_value());
}

void NextApplicationReportJobTests::invalidOutputReferencesDoNotMutateState()
{
    ReportJobState state;
    QVERIFY(state.start(1));
    QVERIFY(state.reportProgress(1));

    const ReportJobSnapshot before = state.snapshot();
    const auto blank = state.complete(" \t\r\n");
    QVERIFY(!blank);
    QCOMPARE(blank.error().code, ErrorCode::InvalidInput);
    QVERIFY(state.snapshot() == before);

    const auto empty = state.complete("");
    QVERIFY(!empty);
    QCOMPARE(empty.error().code, ErrorCode::InvalidInput);
    QVERIFY(state.snapshot() == before);

    const std::string oversized(
        kReportJobMaxOutputReferenceLength + 1,
        'x'
        );
    const auto tooLong = state.complete(oversized);
    QVERIFY(!tooLong);
    QCOMPARE(tooLong.error().code, ErrorCode::InvalidInput);
    QVERIFY(state.snapshot() == before);

    const std::string atLimit(
        kReportJobMaxOutputReferenceLength,
        'x'
        );
    QVERIFY(state.complete(atLimit));
    QCOMPARE(state.snapshot().phase(), ReportJobPhase::Completed);
    QVERIFY(state.snapshot().outputReference().has_value());
    QCOMPARE(*state.snapshot().outputReference(), atLimit);
}

void NextApplicationReportJobTests::zeroUnitJobCompletesWithOutputReference()
{
    ReportJobState state;

    QVERIFY(state.start(0));
    const ReportJobSnapshot started = state.snapshot();
    QCOMPARE(started.phase(), ReportJobPhase::Running);
    QCOMPARE(started.totalUnits(), ReportJobSnapshot::UnitCount{0});
    QCOMPARE(started.completedUnits(), ReportJobSnapshot::UnitCount{0});

    QVERIFY(state.complete("report-token"));
    const ReportJobSnapshot completed = state.snapshot();
    QCOMPARE(completed.phase(), ReportJobPhase::Completed);
    QCOMPARE(completed.totalUnits(), ReportJobSnapshot::UnitCount{0});
    QCOMPARE(completed.completedUnits(), ReportJobSnapshot::UnitCount{0});
    QCOMPARE(
        completed.outputReference(),
        std::optional<std::string>("report-token")
        );
}

void NextApplicationReportJobTests::failurePreservesStructuredError()
{
    ReportJobState state;
    QVERIFY(state.start(3));
    QVERIFY(state.reportProgress(1));

    const OperationError expected{
        .code = ErrorCode::Technical,
        .message = "The report renderer failed.",
        .recoverable = false
    };
    QVERIFY(state.fail(expected));

    const ReportJobSnapshot failed = state.snapshot();
    QCOMPARE(failed.phase(), ReportJobPhase::Failed);
    QCOMPARE(failed.completedUnits(), ReportJobSnapshot::UnitCount{1});
    QVERIFY(failed.error().has_value());
    QVERIFY(*failed.error() == expected);
    QVERIFY(!failed.outputReference().has_value());
}

void NextApplicationReportJobTests::failedStateRejectsLateEventsAndRestarts()
{
    ReportJobState state;
    QVERIFY(state.start(4));
    QVERIFY(state.reportProgress(2));

    const OperationError expected{
        .code = ErrorCode::Technical,
        .message = "The report failed during rendering.",
        .recoverable = false
    };
    QVERIFY(state.fail(expected));

    const ReportJobSnapshot failed = state.snapshot();
    QCOMPARE(failed.phase(), ReportJobPhase::Failed);
    QCOMPARE(failed.totalUnits(), ReportJobSnapshot::UnitCount{4});
    QCOMPARE(failed.completedUnits(), ReportJobSnapshot::UnitCount{2});
    QVERIFY(failed.error().has_value());
    QVERIFY(*failed.error() == expected);
    QVERIFY(!failed.outputReference().has_value());

    const auto progress = state.reportProgress(3);
    const auto request = state.requestCancellation();
    const auto acknowledge = state.acknowledgeCancellation();
    const auto complete = state.complete("late-report.pdf");
    const auto lateFailure = state.fail(
        OperationError{
            .code = ErrorCode::Technical,
            .message = "A late report failure.",
            .recoverable = false
        }
        );

    QVERIFY(!progress);
    QVERIFY(!request);
    QVERIFY(!acknowledge);
    QVERIFY(!complete);
    QVERIFY(!lateFailure);
    QCOMPARE(progress.error().code, ErrorCode::Conflict);
    QCOMPARE(request.error().code, ErrorCode::Conflict);
    QCOMPARE(acknowledge.error().code, ErrorCode::Conflict);
    QCOMPARE(complete.error().code, ErrorCode::Conflict);
    QCOMPARE(lateFailure.error().code, ErrorCode::Conflict);
    QVERIFY(state.snapshot() == failed);
    QVERIFY(state.snapshot().error().has_value());
    QVERIFY(*state.snapshot().error() == expected);
    QVERIFY(!state.snapshot().outputReference().has_value());

    QVERIFY(state.start(2));
    const ReportJobSnapshot restarted = state.snapshot();
    QCOMPARE(restarted.phase(), ReportJobPhase::Running);
    QCOMPARE(restarted.totalUnits(), ReportJobSnapshot::UnitCount{2});
    QCOMPARE(restarted.completedUnits(), ReportJobSnapshot::UnitCount{0});
    QVERIFY(!restarted.cancellationRequested());
    QVERIFY(!restarted.error().has_value());
    QVERIFY(!restarted.outputReference().has_value());
}

void NextApplicationReportJobTests::terminalStateIsImmutableUntilRestart()
{
    ReportJobState state;
    QVERIFY(state.start(1));
    QVERIFY(state.reportProgress(1));
    QVERIFY(state.complete("report.pdf"));
    const ReportJobSnapshot terminal = state.snapshot();

    const auto progress = state.reportProgress(0);
    const auto request = state.requestCancellation();
    const auto acknowledge = state.acknowledgeCancellation();
    const auto complete = state.complete("late-report.pdf");
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
    const ReportJobSnapshot restarted = state.snapshot();
    QCOMPARE(restarted.phase(), ReportJobPhase::Running);
    QCOMPARE(restarted.totalUnits(), ReportJobSnapshot::UnitCount{2});
    QCOMPARE(restarted.completedUnits(), ReportJobSnapshot::UnitCount{0});
    QVERIFY(!restarted.cancellationRequested());
    QVERIFY(!restarted.error().has_value());
    QVERIFY(!restarted.outputReference().has_value());
}

void NextApplicationReportJobTests::canceledStateRejectsLateEventsAndRestarts()
{
    ReportJobState state;
    QVERIFY(state.start(3));
    QVERIFY(state.reportProgress(1));
    QVERIFY(state.requestCancellation());
    QVERIFY(state.acknowledgeCancellation());

    const ReportJobSnapshot canceled = state.snapshot();
    QCOMPARE(canceled.phase(), ReportJobPhase::Canceled);

    const auto progress = state.reportProgress(2);
    const auto request = state.requestCancellation();
    const auto complete = state.complete("late-report.pdf");
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

    QVERIFY(state.start(2));
    const ReportJobSnapshot restarted = state.snapshot();
    QCOMPARE(restarted.phase(), ReportJobPhase::Running);
    QCOMPARE(restarted.totalUnits(), ReportJobSnapshot::UnitCount{2});
    QCOMPARE(restarted.completedUnits(), ReportJobSnapshot::UnitCount{0});
    QVERIFY(!restarted.cancellationRequested());
    QVERIFY(!restarted.error().has_value());
    QVERIFY(!restarted.outputReference().has_value());
}

void NextApplicationReportJobTests::snapshotsAndOwnersAreCopyableValuesWithEquality()
{
    static_assert(std::is_copy_constructible_v<ReportJobSnapshot>);
    static_assert(std::is_copy_assignable_v<ReportJobSnapshot>);
    static_assert(std::is_copy_constructible_v<ReportJobState>);
    static_assert(std::is_copy_assignable_v<ReportJobState>);

    ReportJobState state;
    QVERIFY(state.start(2));
    QVERIFY(state.reportProgress(1));

    const ReportJobSnapshot original = state.snapshot();
    const ReportJobSnapshot copy = original;
    QVERIFY(copy == original);

    const ReportJobState stateCopy = state;
    QVERIFY(stateCopy.snapshot() == original);

    QVERIFY(state.reportProgress(2));
    QVERIFY(original == copy);
    QCOMPARE(original.completedUnits(), ReportJobSnapshot::UnitCount{1});
    QCOMPARE(state.snapshot().completedUnits(), ReportJobSnapshot::UnitCount{2});
}

void NextApplicationReportJobTests::workerBoundaryUsesOwnerEventsAndBoundedSnapshot()
{
    ReportJobState state;
    QVERIFY(state.start(2));

    // A worker-facing value is a copy; lifecycle changes go through owner
    // events and cannot mutate the earlier projection.
    const ReportJobSnapshot workerView = state.snapshot();
    QVERIFY(state.reportProgress(1));
    QVERIFY(state.requestCancellation());
    QVERIFY(state.acknowledgeCancellation());

    QCOMPARE(workerView.phase(), ReportJobPhase::Running);
    QCOMPARE(workerView.completedUnits(), ReportJobSnapshot::UnitCount{0});
    QVERIFY(!workerView.outputReference().has_value());
    QVERIFY(!state.snapshot().outputReference().has_value());
}

QTEST_APPLESS_MAIN(NextApplicationReportJobTests)

#include "next_application_report_job_tests.moc"
