#include "next/application/import_job_coordinator.h"

#include <QtTest/QtTest>

#include <atomic>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

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

class RecordingWorker final : public ImportJobWorkerPort
{
public:
    [[nodiscard]] Result<void> start(
        const ItemCount totalItems,
        const Generation generation,
        ImportJobEventSink& eventSink
        ) override
    {
        ++startCalls;
        lastTotalItems = totalItems;
        lastGeneration = generation;
        sink = &eventSink;
        if (startError.has_value())
        {
            return Result<void>::failure(*startError);
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
            throw std::runtime_error("cancellation failed");
        case CancellationBehavior::ThrowUnknownException:
            throw 7;
        }

        return Result<void>::success();
    }

    [[nodiscard]] Result<void> post(
        ImportJobEvent event
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
    ItemCount lastTotalItems = 0;
    Generation lastGeneration = 0;
    ImportJobEventSink* sink = nullptr;
    std::optional<OperationError> startError;
    std::optional<OperationError> cancellationError;
    CancellationBehavior cancellationBehavior = CancellationBehavior::Succeed;
};

}

class NextApplicationImportJobCoordinatorTests final : public QObject
{
    Q_OBJECT

private slots:
    void workerOnlyPublishesEventsUntilOwnerPumps();
    void duplicateStartLeavesQueuedEventsAndDoesNotRestartWorker();
    void fifoEventsApplyInPostedOrder();
    void progressAndCompletionMapToState();
    void cancellationRequestForwardsAndAcknowledgementCancels();
    void cancellationFailurePreservesRequestedRunningState();
    void cancellationExceptionsPreserveRequestedRunningState();
    void completionWinsBeforeCancellationAcknowledgement();
    void failureEventPreservesStructuredError();
    void workerStartFailureBecomesExplicitFailedState();
    void staleGenerationCannotMutateRestartedJob();
    void queueOverflowIsBoundedAndThreadSafe();
    void zeroCapacityQueueRejectsWithoutChangingState();
    void terminalStateRemainsImmutableForLateEvents();
    void workerBoundaryIsAbstractAndCopyableEventsCarryNoWorkerState();
};

void NextApplicationImportJobCoordinatorTests::workerOnlyPublishesEventsUntilOwnerPumps()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker, 4);

    const auto start = coordinator.startJob(3);
    QVERIFY(start);
    QVERIFY(worker.sink != nullptr);
    QCOMPARE(worker.lastGeneration, ImportJobGeneration{1});

    QVERIFY(worker.post(ImportJobEvent::progress(start.value(), 1)));
    QCOMPARE(coordinator.snapshot().completedItems(), ImportJobSnapshot::ItemCount{0});
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{1});

    const auto pump = coordinator.pump();
    QVERIFY(pump);
    QCOMPARE(pump.value(), std::size_t{1});
    QCOMPARE(coordinator.snapshot().completedItems(), ImportJobSnapshot::ItemCount{1});
}

void NextApplicationImportJobCoordinatorTests::duplicateStartLeavesQueuedEventsAndDoesNotRestartWorker()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker, 4);
    const auto first = coordinator.startJob(3);
    QVERIFY(first);
    QVERIFY(worker.post(ImportJobEvent::progress(first.value(), 1)));

    const auto duplicate = coordinator.startJob(5);

    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, ErrorCode::Conflict);
    QVERIFY(duplicate.error().recoverable);
    QCOMPARE(worker.startCalls, 1);
    QCOMPARE(coordinator.generation(), first.value());
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{1});
    QVERIFY(coordinator.pump());
    QCOMPARE(coordinator.snapshot().totalItems(), ImportJobSnapshot::ItemCount{3});
    QCOMPARE(coordinator.snapshot().completedItems(), ImportJobSnapshot::ItemCount{1});
}

void NextApplicationImportJobCoordinatorTests::fifoEventsApplyInPostedOrder()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker, 4);
    const auto start = coordinator.startJob(3);
    QVERIFY(start);

    QVERIFY(worker.post(ImportJobEvent::progress(start.value(), 2)));
    QVERIFY(worker.post(ImportJobEvent::progress(start.value(), 1)));

    const auto pump = coordinator.pumpEvents();
    QVERIFY(pump);
    QCOMPARE(pump.value(), std::size_t{2});
    QCOMPARE(coordinator.snapshot().completedItems(), ImportJobSnapshot::ItemCount{1});
}

void NextApplicationImportJobCoordinatorTests::progressAndCompletionMapToState()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker);
    const auto start = coordinator.startJob(2);
    QVERIFY(start);

    QVERIFY(worker.post(ImportJobEvent::progress(start.value(), 2)));
    QVERIFY(worker.post(ImportJobEvent::completed(start.value())));
    QVERIFY(coordinator.pump());

    const ImportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ImportJobPhase::Completed);
    QCOMPARE(snapshot.totalItems(), ImportJobSnapshot::ItemCount{2});
    QCOMPARE(snapshot.completedItems(), ImportJobSnapshot::ItemCount{2});
}

void NextApplicationImportJobCoordinatorTests::cancellationRequestForwardsAndAcknowledgementCancels()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker);
    const auto start = coordinator.startJob(4);
    QVERIFY(start);

    QVERIFY(coordinator.requestCancellation());
    QCOMPARE(worker.cancellationCalls, 1);
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Running);
    QVERIFY(coordinator.snapshot().cancellationRequested());

    QVERIFY(worker.post(ImportJobEvent::cancellationAcknowledged(start.value())));
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Running);
    QVERIFY(coordinator.pump());
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Canceled);
}

void NextApplicationImportJobCoordinatorTests::cancellationFailurePreservesRequestedRunningState()
{
    RecordingWorker worker;
    const OperationError expected = workerError("The worker refused cancellation.");
    worker.cancellationBehavior = CancellationBehavior::Fail;
    worker.cancellationError = expected;
    ImportJobCoordinator coordinator(worker);
    QVERIFY(coordinator.startJob(4));

    const auto result = coordinator.requestCancellation();

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(worker.cancellationCalls, 1);
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Running);
    QVERIFY(coordinator.snapshot().cancellationRequested());
}

void NextApplicationImportJobCoordinatorTests::cancellationExceptionsPreserveRequestedRunningState()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker);
    QVERIFY(coordinator.startJob(4));

    worker.cancellationBehavior = CancellationBehavior::ThrowStandardException;
    const auto standardException = coordinator.requestCancellation();
    QVERIFY(!standardException);
    QCOMPARE(standardException.error().code, ErrorCode::Technical);
    QVERIFY(standardException.error().message == "Import worker cancellation raised an exception.");
    QVERIFY(!standardException.error().recoverable);
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Running);
    QVERIFY(coordinator.snapshot().cancellationRequested());

    worker.cancellationBehavior = CancellationBehavior::ThrowUnknownException;
    const auto unknownException = coordinator.requestCancellation();
    QVERIFY(!unknownException);
    QCOMPARE(unknownException.error().code, ErrorCode::Technical);
    QVERIFY(unknownException.error().message == "Import worker cancellation raised an unknown exception.");
    QVERIFY(!unknownException.error().recoverable);
    QCOMPARE(worker.cancellationCalls, 2);
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Running);
    QVERIFY(coordinator.snapshot().cancellationRequested());
}

void NextApplicationImportJobCoordinatorTests::completionWinsBeforeCancellationAcknowledgement()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker);
    const auto start = coordinator.startJob(2);
    QVERIFY(start);
    QVERIFY(coordinator.requestCancellation());

    QVERIFY(worker.post(ImportJobEvent::progress(start.value(), 2)));
    QVERIFY(worker.post(ImportJobEvent::completed(start.value())));
    QVERIFY(worker.post(ImportJobEvent::cancellationAcknowledged(start.value())));
    QVERIFY(coordinator.pump());

    const ImportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ImportJobPhase::Completed);
    QVERIFY(snapshot.cancellationRequested());
}

void NextApplicationImportJobCoordinatorTests::failureEventPreservesStructuredError()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker);
    const auto start = coordinator.startJob(3);
    QVERIFY(start);
    const OperationError expected = workerError("The import parser failed.");

    QVERIFY(worker.post(ImportJobEvent::progress(start.value(), 1)));
    QVERIFY(worker.post(ImportJobEvent::failed(start.value(), expected)));
    QVERIFY(coordinator.pump());

    const ImportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ImportJobPhase::Failed);
    QCOMPARE(snapshot.completedItems(), ImportJobSnapshot::ItemCount{1});
    QVERIFY(snapshot.error().has_value());
    QVERIFY(*snapshot.error() == expected);
}

void NextApplicationImportJobCoordinatorTests::workerStartFailureBecomesExplicitFailedState()
{
    RecordingWorker worker;
    const OperationError expected = workerError("The worker could not start.");
    worker.startError = expected;
    ImportJobCoordinator coordinator(worker);

    const auto result = coordinator.startJob(5);

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    const ImportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ImportJobPhase::Failed);
    QVERIFY(snapshot.error().has_value());
    QVERIFY(*snapshot.error() == expected);
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{0});

    worker.startError.reset();
    const auto restarted = coordinator.startJob(1);
    QVERIFY(restarted);
    QCOMPARE(restarted.value(), ImportJobGeneration{2});
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Running);
}

void NextApplicationImportJobCoordinatorTests::staleGenerationCannotMutateRestartedJob()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker, 4);
    const auto first = coordinator.startJob(1);
    QVERIFY(first);
    QVERIFY(worker.post(ImportJobEvent::progress(first.value(), 1)));
    QVERIFY(worker.post(ImportJobEvent::completed(first.value())));
    QVERIFY(coordinator.pump());

    // Events retained at the explicit restart boundary are discarded.
    QVERIFY(worker.post(ImportJobEvent::failed(first.value(), workerError("old"))));
    const auto second = coordinator.startJob(4);
    QVERIFY(second);
    QCOMPARE(second.value(), ImportJobGeneration{2});
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{0});

    // An old worker may still publish after the new worker has started.
    QVERIFY(worker.post(ImportJobEvent::failed(first.value(), workerError("late old"))));
    QVERIFY(worker.post(ImportJobEvent::progress(second.value(), 1)));
    QVERIFY(coordinator.pump());

    const ImportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ImportJobPhase::Running);
    QCOMPARE(snapshot.totalItems(), ImportJobSnapshot::ItemCount{4});
    QCOMPARE(snapshot.completedItems(), ImportJobSnapshot::ItemCount{1});
    QVERIFY(!snapshot.error().has_value());
}

void NextApplicationImportJobCoordinatorTests::queueOverflowIsBoundedAndThreadSafe()
{
    ImportJobEventQueue queue(2);
    QCOMPARE(queue.capacity(), std::size_t{2});
    QVERIFY(queue.post(ImportJobEvent::progress(1, 1)));
    QVERIFY(queue.post(ImportJobEvent::progress(1, 2)));

    const auto overflow = queue.post(ImportJobEvent::progress(1, 3));
    QVERIFY(!overflow);
    QCOMPARE(overflow.error().code, ErrorCode::Conflict);
    QVERIFY(overflow.error().recoverable);
    QCOMPARE(queue.size(), std::size_t{2});

    const auto events = queue.drain();
    QCOMPARE(events.size(), std::size_t{2});
    QCOMPARE(events[0].completedItems, ImportJobSnapshot::ItemCount{1});
    QCOMPARE(events[1].completedItems, ImportJobSnapshot::ItemCount{2});

    ImportJobEventQueue concurrentQueue(3);
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
                            ImportJobEvent::progress(
                                1,
                                static_cast<ImportJobSnapshot::ItemCount>(
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

void NextApplicationImportJobCoordinatorTests::zeroCapacityQueueRejectsWithoutChangingState()
{
    ImportJobEventQueue queue(0);
    const ImportJobEvent event = ImportJobEvent::progress(1, 1);

    const auto result = queue.post(event);

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QVERIFY(result.error().recoverable);
    QCOMPARE(queue.capacity(), std::size_t{0});
    QCOMPARE(queue.size(), std::size_t{0});
    QVERIFY(queue.empty());
    QVERIFY(queue.drain().empty());
    QVERIFY(!queue.tryPop().has_value());
}

void NextApplicationImportJobCoordinatorTests::terminalStateRemainsImmutableForLateEvents()
{
    RecordingWorker worker;
    ImportJobCoordinator coordinator(worker, 8);
    const auto start = coordinator.startJob(1);
    QVERIFY(start);
    QVERIFY(worker.post(ImportJobEvent::progress(start.value(), 1)));
    QVERIFY(worker.post(ImportJobEvent::completed(start.value())));
    QVERIFY(coordinator.pump());
    const ImportJobSnapshot terminal = coordinator.snapshot();

    QVERIFY(worker.post(ImportJobEvent::progress(start.value(), 0)));
    QVERIFY(worker.post(ImportJobEvent::cancellationAcknowledged(start.value())));
    QVERIFY(worker.post(ImportJobEvent::failed(start.value(), workerError("late"))));
    QVERIFY(coordinator.pump());

    QCOMPARE(coordinator.snapshot(), terminal);
}

void NextApplicationImportJobCoordinatorTests::workerBoundaryIsAbstractAndCopyableEventsCarryNoWorkerState()
{
    static_assert(std::is_abstract_v<ImportJobWorkerPort>);
    static_assert(std::is_copy_constructible_v<ImportJobEvent>);
    static_assert(std::is_copy_assignable_v<ImportJobEvent>);

    const ImportJobEvent event = ImportJobEvent::progress(7, 3);
    QCOMPARE(event.generation, ImportJobGeneration{7});
    QCOMPARE(event.completedItems, ImportJobSnapshot::ItemCount{3});
    QCOMPARE(event.kind, ImportJobEventKind::Progress);
}

QTEST_APPLESS_MAIN(NextApplicationImportJobCoordinatorTests)

#include "next_application_import_job_coordinator_tests.moc"
