#include "next/application/report_job_coordinator.h"

#include <QtTest/QtTest>

#include <atomic>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

enum class WorkerStartBehavior
{
    Succeed,
    Fail,
    ThrowStandardException,
    ThrowUnknownException
};

enum class CancellationBehavior
{
    Succeed,
    Fail,
    ThrowStandardException,
    ThrowUnknownException
};

OperationError workerError(
    const char* message
    )
{
    return OperationError{
        .code = ErrorCode::Technical,
        .message = message,
        .recoverable = false
    };
}

class RecordingWorker final : public ReportJobWorkerPort
{
public:
    [[nodiscard]] Result<void> start(
        const UnitCount totalUnits,
        const Generation generation,
        ReportJobEventSink& eventSink
        ) override
    {
        ++startCalls;
        lastTotalUnits = totalUnits;
        lastGeneration = generation;
        sink = &eventSink;

        switch (startBehavior)
        {
        case WorkerStartBehavior::Succeed:
            return Result<void>::success();
        case WorkerStartBehavior::Fail:
            return Result<void>::failure(*startError);
        case WorkerStartBehavior::ThrowStandardException:
            throw std::runtime_error("report worker start failed");
        case WorkerStartBehavior::ThrowUnknownException:
            throw 7;
        }

        return Result<void>::success();
    }

    [[nodiscard]] Result<void> requestCancellation() override
    {
        ++cancellationCalls;
        switch (cancellationBehavior)
        {
        case CancellationBehavior::Succeed:
            return Result<void>::success();
        case CancellationBehavior::Fail:
            return Result<void>::failure(*cancellationError);
        case CancellationBehavior::ThrowStandardException:
            throw std::runtime_error("report worker cancellation failed");
        case CancellationBehavior::ThrowUnknownException:
            throw 7;
        }

        return Result<void>::success();
    }

    [[nodiscard]] Result<void> post(
        ReportJobEvent event
        ) const
    {
        if (sink == nullptr)
        {
            return Result<void>::failure(
                OperationError{
                    .code = ErrorCode::Conflict,
                    .message = "Worker event sink has not been assigned.",
                    .recoverable = true
                }
                );
        }

        return sink->post(std::move(event));
    }

    int startCalls = 0;
    int cancellationCalls = 0;
    UnitCount lastTotalUnits = 0;
    Generation lastGeneration = 0;
    ReportJobEventSink* sink = nullptr;
    WorkerStartBehavior startBehavior = WorkerStartBehavior::Succeed;
    CancellationBehavior cancellationBehavior = CancellationBehavior::Succeed;
    std::optional<OperationError> startError;
    std::optional<OperationError> cancellationError;
};

}

class NextApplicationReportJobCoordinatorTests final : public QObject
{
    Q_OBJECT

private slots:
    void workerOnlyPublishesEventsUntilOwnerPumps();
    void duplicateStartPreservesStateAndQueuedEvents();
    void startResetsStateAndClearsQueuedEvents();
    void fifoEventsApplyInPostedOrder();
    void progressAndCompletionCarryOutputReference();
    void invalidOutputAndIncompleteCompletionDoNotClaimSuccess();
    void cancellationRequestForwardsAndAcknowledgementCancels();
    void cancellationFailurePreservesRequestedRunningState();
    void cancellationExceptionsPreserveRequestedRunningState();
    void completionWinsBeforeCancellationAcknowledgement();
    void workerStartFailureBecomesExplicitFailedState();
    void workerStartExceptionsBecomeTechnicalFailedStates();
    void staleGenerationCannotMutateRestartedJob();
    void queueOverflowIsBoundedAndThreadSafe();
    void zeroCapacityQueueRejectsWithoutChangingState();
    void terminalStateRemainsImmutableForLateEvents();
    void workerBoundaryIsAbstractAndCopyableEventsCarryNoWorkerState();
};

void NextApplicationReportJobCoordinatorTests::workerOnlyPublishesEventsUntilOwnerPumps()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker, 4);

    const auto start = coordinator.startJob(3);
    QVERIFY(start);
    QVERIFY(worker.sink != nullptr);
    QCOMPARE(worker.lastTotalUnits, ReportJobSnapshot::UnitCount{3});
    QCOMPARE(worker.lastGeneration, ReportJobGeneration{1});

    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 1)));
    QCOMPARE(coordinator.snapshot().completedUnits(), ReportJobSnapshot::UnitCount{0});
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{1});

    const auto pump = coordinator.pump();
    QVERIFY(pump);
    QCOMPARE(pump.value(), std::size_t{1});
    QCOMPARE(coordinator.snapshot().completedUnits(), ReportJobSnapshot::UnitCount{1});
}

void NextApplicationReportJobCoordinatorTests::duplicateStartPreservesStateAndQueuedEvents()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker, 4);
    const auto first = coordinator.startJob(3);
    QVERIFY(first);
    QVERIFY(worker.post(ReportJobEvent::progress(first.value(), 1)));

    const ReportJobSnapshot before = coordinator.snapshot();
    const auto duplicate = coordinator.startJob(5);

    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, ErrorCode::Conflict);
    QVERIFY(duplicate.error().recoverable);
    QCOMPARE(worker.startCalls, 1);
    QCOMPARE(coordinator.generation(), first.value());
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{1});
    QCOMPARE(coordinator.snapshot(), before);
    QVERIFY(coordinator.pump());
    QCOMPARE(coordinator.snapshot().totalUnits(), ReportJobSnapshot::UnitCount{3});
    QCOMPARE(coordinator.snapshot().completedUnits(), ReportJobSnapshot::UnitCount{1});
}

void NextApplicationReportJobCoordinatorTests::startResetsStateAndClearsQueuedEvents()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker, 4);
    const auto first = coordinator.startJob(1);
    QVERIFY(first);
    QVERIFY(worker.post(ReportJobEvent::progress(first.value(), 1)));
    QVERIFY(worker.post(ReportJobEvent::completed(first.value(), "first.pdf")));
    QVERIFY(coordinator.pump());
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Completed);

    QVERIFY(worker.post(ReportJobEvent::failed(first.value(), workerError("old"))));
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{1});

    const auto second = coordinator.startJob(4);

    QVERIFY(second);
    QCOMPARE(second.value(), ReportJobGeneration{2});
    QCOMPARE(worker.startCalls, 2);
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{0});
    const ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Running);
    QCOMPARE(snapshot.totalUnits(), ReportJobSnapshot::UnitCount{4});
    QCOMPARE(snapshot.completedUnits(), ReportJobSnapshot::UnitCount{0});
    QVERIFY(!snapshot.cancellationRequested());
    QVERIFY(!snapshot.error().has_value());
    QVERIFY(!snapshot.outputReference().has_value());
}

void NextApplicationReportJobCoordinatorTests::fifoEventsApplyInPostedOrder()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker, 4);
    const auto start = coordinator.startJob(3);
    QVERIFY(start);

    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 2)));
    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 1)));

    const auto pump = coordinator.pumpEvents();
    QVERIFY(pump);
    QCOMPARE(pump.value(), std::size_t{2});
    QCOMPARE(coordinator.snapshot().completedUnits(), ReportJobSnapshot::UnitCount{1});
}

void NextApplicationReportJobCoordinatorTests::progressAndCompletionCarryOutputReference()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker);
    const auto start = coordinator.startJob(2);
    QVERIFY(start);

    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 2)));
    QVERIFY(worker.post(ReportJobEvent::completed(start.value(), "reports/report.pdf")));
    QVERIFY(coordinator.pump());

    const ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Completed);
    QCOMPARE(snapshot.totalUnits(), ReportJobSnapshot::UnitCount{2});
    QCOMPARE(snapshot.completedUnits(), ReportJobSnapshot::UnitCount{2});
    QVERIFY(snapshot.outputReference().has_value());
    QCOMPARE(*snapshot.outputReference(), std::string("reports/report.pdf"));
    QVERIFY(!snapshot.error().has_value());
}

void NextApplicationReportJobCoordinatorTests::invalidOutputAndIncompleteCompletionDoNotClaimSuccess()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker, 8);
    const auto start = coordinator.startJob(2);
    QVERIFY(start);

    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 1)));
    QVERIFY(worker.post(ReportJobEvent::completed(start.value(), "")));
    QVERIFY(worker.post(
        ReportJobEvent::completed(
            start.value(),
            std::string(kReportJobMaxOutputReferenceLength + 1, 'x')
            )
        ));
    QVERIFY(worker.post(ReportJobEvent::completed(start.value(), "incomplete.pdf")));
    QVERIFY(coordinator.pump());

    ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Running);
    QCOMPARE(snapshot.completedUnits(), ReportJobSnapshot::UnitCount{1});
    QVERIFY(!snapshot.outputReference().has_value());

    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 2)));
    QVERIFY(worker.post(ReportJobEvent::completed(start.value(), "complete.pdf")));
    QVERIFY(coordinator.pump());

    snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Completed);
    QVERIFY(snapshot.outputReference().has_value());
    QCOMPARE(*snapshot.outputReference(), std::string("complete.pdf"));
}

void NextApplicationReportJobCoordinatorTests::cancellationRequestForwardsAndAcknowledgementCancels()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker);
    const auto start = coordinator.startJob(4);
    QVERIFY(start);

    QVERIFY(coordinator.requestCancellation());
    QCOMPARE(worker.cancellationCalls, 1);
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Running);
    QVERIFY(coordinator.snapshot().cancellationRequested());

    QVERIFY(worker.post(ReportJobEvent::cancellationAcknowledged(start.value())));
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Running);
    QVERIFY(coordinator.pump());
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Canceled);
    QVERIFY(coordinator.snapshot().cancellationRequested());
}

void NextApplicationReportJobCoordinatorTests::cancellationFailurePreservesRequestedRunningState()
{
    RecordingWorker worker;
    const OperationError expected = workerError("The worker refused report cancellation.");
    worker.cancellationBehavior = CancellationBehavior::Fail;
    worker.cancellationError = expected;
    ReportJobCoordinator coordinator(worker);
    QVERIFY(coordinator.startJob(4));

    const auto result = coordinator.requestCancellation();

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(worker.cancellationCalls, 1);
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Running);
    QVERIFY(coordinator.snapshot().cancellationRequested());
}

void NextApplicationReportJobCoordinatorTests::cancellationExceptionsPreserveRequestedRunningState()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker);
    QVERIFY(coordinator.startJob(4));

    worker.cancellationBehavior = CancellationBehavior::ThrowStandardException;
    const auto standardException = coordinator.requestCancellation();
    QVERIFY(!standardException);
    QCOMPARE(standardException.error().code, ErrorCode::Technical);
    QVERIFY(standardException.error().message == "Report worker cancellation raised an exception.");
    QVERIFY(!standardException.error().recoverable);
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Running);
    QVERIFY(coordinator.snapshot().cancellationRequested());

    worker.cancellationBehavior = CancellationBehavior::ThrowUnknownException;
    const auto unknownException = coordinator.requestCancellation();
    QVERIFY(!unknownException);
    QCOMPARE(unknownException.error().code, ErrorCode::Technical);
    QVERIFY(unknownException.error().message == "Report worker cancellation raised an unknown exception.");
    QVERIFY(!unknownException.error().recoverable);
    QCOMPARE(worker.cancellationCalls, 2);
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Running);
    QVERIFY(coordinator.snapshot().cancellationRequested());
}

void NextApplicationReportJobCoordinatorTests::completionWinsBeforeCancellationAcknowledgement()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker);
    const auto start = coordinator.startJob(2);
    QVERIFY(start);
    QVERIFY(coordinator.requestCancellation());

    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 2)));
    QVERIFY(worker.post(ReportJobEvent::completed(start.value(), "race.pdf")));
    QVERIFY(worker.post(ReportJobEvent::cancellationAcknowledged(start.value())));
    QVERIFY(coordinator.pump());

    const ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Completed);
    QVERIFY(snapshot.cancellationRequested());
    QVERIFY(snapshot.outputReference().has_value());
    QCOMPARE(*snapshot.outputReference(), std::string("race.pdf"));
}

void NextApplicationReportJobCoordinatorTests::workerStartFailureBecomesExplicitFailedState()
{
    RecordingWorker worker;
    const OperationError expected = workerError("The report worker could not start.");
    worker.startBehavior = WorkerStartBehavior::Fail;
    worker.startError = expected;
    ReportJobCoordinator coordinator(worker);

    const auto result = coordinator.startJob(5);

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    const ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Failed);
    QVERIFY(snapshot.error().has_value());
    QVERIFY(*snapshot.error() == expected);
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{0});

    worker.startBehavior = WorkerStartBehavior::Succeed;
    worker.startError.reset();
    const auto restarted = coordinator.startJob(1);
    QVERIFY(restarted);
    QCOMPARE(restarted.value(), ReportJobGeneration{2});
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Running);
}

void NextApplicationReportJobCoordinatorTests::workerStartExceptionsBecomeTechnicalFailedStates()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker);

    worker.startBehavior = WorkerStartBehavior::ThrowStandardException;
    const auto standardException = coordinator.startJob(2);
    QVERIFY(!standardException);
    QCOMPARE(standardException.error().code, ErrorCode::Technical);
    QCOMPARE(standardException.error().message, std::string("Report worker start raised an exception."));
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Failed);
    QVERIFY(coordinator.snapshot().error().has_value());
    QCOMPARE(*coordinator.snapshot().error(), standardException.error());

    worker.startBehavior = WorkerStartBehavior::ThrowUnknownException;
    const auto unknownException = coordinator.startJob(2);
    QVERIFY(!unknownException);
    QCOMPARE(unknownException.error().code, ErrorCode::Technical);
    QCOMPARE(unknownException.error().message, std::string("Report worker start raised an unknown exception."));
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Failed);
    QVERIFY(coordinator.snapshot().error().has_value());
    QCOMPARE(*coordinator.snapshot().error(), unknownException.error());
}

void NextApplicationReportJobCoordinatorTests::staleGenerationCannotMutateRestartedJob()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker, 8);
    const auto first = coordinator.startJob(1);
    QVERIFY(first);
    QVERIFY(worker.post(ReportJobEvent::progress(first.value(), 1)));
    QVERIFY(worker.post(ReportJobEvent::completed(first.value(), "first.pdf")));
    QVERIFY(coordinator.pump());

    // Events retained at the explicit restart boundary are discarded.
    QVERIFY(worker.post(ReportJobEvent::failed(first.value(), workerError("old"))));
    const auto second = coordinator.startJob(4);
    QVERIFY(second);
    QCOMPARE(second.value(), ReportJobGeneration{2});
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{0});

    // An old worker may still publish after the new worker has started.
    QVERIFY(worker.post(ReportJobEvent::failed(first.value(), workerError("late old"))));
    QVERIFY(worker.post(ReportJobEvent::progress(first.value(), 1)));
    QVERIFY(worker.post(ReportJobEvent::progress(second.value(), 1)));
    const auto pump = coordinator.pump();
    QVERIFY(pump);
    QCOMPARE(pump.value(), std::size_t{3});

    const ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Running);
    QCOMPARE(snapshot.totalUnits(), ReportJobSnapshot::UnitCount{4});
    QCOMPARE(snapshot.completedUnits(), ReportJobSnapshot::UnitCount{1});
    QVERIFY(!snapshot.error().has_value());
}

void NextApplicationReportJobCoordinatorTests::queueOverflowIsBoundedAndThreadSafe()
{
    ReportJobEventQueue queue(2);
    QCOMPARE(queue.capacity(), std::size_t{2});
    QVERIFY(queue.post(ReportJobEvent::progress(1, 1)));
    QVERIFY(queue.post(ReportJobEvent::progress(1, 2)));

    const auto overflow = queue.post(ReportJobEvent::progress(1, 3));
    QVERIFY(!overflow);
    QCOMPARE(overflow.error().code, ErrorCode::Conflict);
    QVERIFY(overflow.error().recoverable);
    QCOMPARE(queue.size(), std::size_t{2});

    const auto events = queue.drain();
    QCOMPARE(events.size(), std::size_t{2});
    QCOMPARE(events[0].completedUnits, ReportJobSnapshot::UnitCount{1});
    QCOMPARE(events[1].completedUnits, ReportJobSnapshot::UnitCount{2});

    ReportJobEventQueue concurrentQueue(3);
    std::atomic<int> accepted{0};
    std::vector<std::thread> producers;
    for (int producer = 0; producer < 4; ++producer)
    {
        producers.emplace_back(
            [&concurrentQueue, &accepted, producer]
            {
                for (int event = 0; event < 4; ++event)
                {
                    if (concurrentQueue.post(
                            ReportJobEvent::progress(
                                1,
                                static_cast<ReportJobSnapshot::UnitCount>(
                                    producer * 4 + event
                                    )
                                )
                            ))
                    {
                        ++accepted;
                    }
                }
            }
            );
    }
    for (std::thread& producer : producers)
    {
        producer.join();
    }

    QCOMPARE(accepted.load(), 3);
    QCOMPARE(concurrentQueue.size(), std::size_t{3});
}

void NextApplicationReportJobCoordinatorTests::zeroCapacityQueueRejectsWithoutChangingState()
{
    ReportJobEventQueue queue(0);
    const ReportJobEvent event = ReportJobEvent::progress(1, 1);

    const auto result = queue.post(event);

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QVERIFY(result.error().recoverable);
    QCOMPARE(queue.capacity(), std::size_t{0});
    QCOMPARE(queue.size(), std::size_t{0});
    QVERIFY(queue.empty());
    QVERIFY(queue.drain().empty());
    QVERIFY(!queue.tryPop().has_value());

    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker, 0);
    const auto start = coordinator.startJob(1);
    QVERIFY(start);
    QVERIFY(!worker.post(ReportJobEvent::progress(start.value(), 1)));
    QVERIFY(coordinator.pump());
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Running);
    QCOMPARE(coordinator.snapshot().completedUnits(), ReportJobSnapshot::UnitCount{0});
}

void NextApplicationReportJobCoordinatorTests::terminalStateRemainsImmutableForLateEvents()
{
    RecordingWorker worker;
    ReportJobCoordinator coordinator(worker, 8);
    const auto start = coordinator.startJob(1);
    QVERIFY(start);
    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 1)));
    QVERIFY(worker.post(ReportJobEvent::completed(start.value(), "report.pdf")));
    QVERIFY(coordinator.pump());
    const ReportJobSnapshot terminal = coordinator.snapshot();

    QVERIFY(worker.post(ReportJobEvent::progress(start.value(), 0)));
    QVERIFY(worker.post(ReportJobEvent::cancellationAcknowledged(start.value())));
    QVERIFY(worker.post(ReportJobEvent::failed(start.value(), workerError("late"))));
    QVERIFY(worker.post(ReportJobEvent::completed(start.value(), "late-report.pdf")));
    QVERIFY(coordinator.pump());

    QCOMPARE(coordinator.snapshot(), terminal);
}

void NextApplicationReportJobCoordinatorTests::workerBoundaryIsAbstractAndCopyableEventsCarryNoWorkerState()
{
    static_assert(std::is_abstract_v<ReportJobWorkerPort>);
    static_assert(std::is_abstract_v<ReportJobEventSink>);
    static_assert(std::is_copy_constructible_v<ReportJobEvent>);
    static_assert(std::is_copy_assignable_v<ReportJobEvent>);
    static_assert(std::is_copy_constructible_v<ReportJobSnapshot>);
    static_assert(std::is_copy_assignable_v<ReportJobSnapshot>);

    const ReportJobEvent event = ReportJobEvent::completed(7, "report.pdf");
    QCOMPARE(event.generation, ReportJobGeneration{7});
    QCOMPARE(event.kind, ReportJobEventKind::Completed);
    QCOMPARE(event.outputReference, std::string("report.pdf"));
    QCOMPARE(event.completedUnits, ReportJobSnapshot::UnitCount{0});
}

QTEST_APPLESS_MAIN(NextApplicationReportJobCoordinatorTests)

#include "next_application_report_job_coordinator_tests.moc"
