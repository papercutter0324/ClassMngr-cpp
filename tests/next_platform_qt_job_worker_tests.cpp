#include "next/platform/qt_job_worker_adapters.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QWaitCondition>
#include <QtTest>

#include <array>
#include <stdexcept>
#include <string>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using namespace ClassMngr::Next::Platform;

namespace
{

constexpr int kTimeoutMs = 5'000;

[[nodiscard]] bool waitForFlag(
    QMutex& mutex,
    QWaitCondition& condition,
    const bool& flag
    )
{
    QMutexLocker locker(&mutex);
    QDeadlineTimer deadline(kTimeoutMs);
    while (!flag)
    {
        if (!condition.wait(&mutex, deadline))
        {
            return flag;
        }
    }
    return true;
}

[[nodiscard]] OperationError taskError(const char* message)
{
    return OperationError{
        .code = ErrorCode::Validation,
        .message = message,
        .recoverable = false
    };
}

}

class NextPlatformQtJobWorkerTests final : public QObject
{
    Q_OBJECT

private slots:
    void importWorkerRunsOffCallerAndCompletesThroughCoordinator();
    void reportWorkerRunsOffCallerAndCompletesWithOutputReference();
    void reportWorkerRejectsInvalidOutputReferencesAsFailures();
    void reportCancellationIsAcknowledgedAfterCooperativeWorkReturns();
    void reportCancellationWinsOverInvalidOutputReference();
    void importCancellationIsAcknowledgedAfterCooperativeWorkReturns();
    void taskFailureAndExceptionProduceFailedEvents();
    void zeroCapacityQueueMakesLastResultReportPostFailure();
    void reportZeroCapacityQueueMakesLastResultReportPostFailure();
    void duplicateActiveStartIsRejectedAndFinishedWorkerCanRestart();
    void completedJobRemainsCompletedAfterLaterCancellationRequest();
    void destructorRequestsCancellationAndJoinsCooperativeWork();
};

void NextPlatformQtJobWorkerTests::
    importWorkerRunsOffCallerAndCompletesThroughCoordinator()
{
    const Qt::HANDLE callerThread = QThread::currentThreadId();
    Qt::HANDLE workerThread = callerThread;
    ImportJobSnapshot::ItemCount receivedCount = 0;
    ImportJobGeneration receivedGeneration;

    QtImportJobWorker worker(
        [&](const auto count, const auto generation, auto&, const auto&)
        {
            workerThread = QThread::currentThreadId();
            receivedCount = count;
            receivedGeneration = generation;
            return Result<void>::success();
        }
        );
    ImportJobCoordinator coordinator(worker);

    const auto start = coordinator.startJob(3);
    QVERIFY(start);
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();

    QVERIFY(workerThread != callerThread);
    QCOMPARE(receivedCount, ImportJobSnapshot::ItemCount{3});
    QVERIFY(receivedGeneration == start.value());
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{2});
    const auto pump = coordinator.pump();
    QVERIFY(pump);
    QCOMPARE(pump.value(), std::size_t{2});
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Completed);
    QCOMPARE(
        coordinator.snapshot().completedItems(),
        ImportJobSnapshot::ItemCount{3}
        );
}

void NextPlatformQtJobWorkerTests::
    reportWorkerRunsOffCallerAndCompletesWithOutputReference()
{
    const Qt::HANDLE callerThread = QThread::currentThreadId();
    Qt::HANDLE workerThread = callerThread;
    ReportJobSnapshot::UnitCount receivedCount = 0;
    ReportJobGeneration receivedGeneration;
    const std::string outputReference = "reports/worker-output.pdf";

    QtReportJobWorker worker(
        [&](const auto count, const auto generation, auto&, const auto&)
        {
            workerThread = QThread::currentThreadId();
            receivedCount = count;
            receivedGeneration = generation;
            return Result<std::string>::success(outputReference);
        }
        );
    ReportJobCoordinator coordinator(worker);

    const auto start = coordinator.startJob(4);
    QVERIFY(start);
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();

    QVERIFY(workerThread != callerThread);
    QCOMPARE(receivedCount, ReportJobSnapshot::UnitCount{4});
    QVERIFY(receivedGeneration == start.value());
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{2});
    QVERIFY(coordinator.pump());
    const ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Completed);
    QCOMPARE(snapshot.completedUnits(), ReportJobSnapshot::UnitCount{4});
    QVERIFY(snapshot.outputReference().has_value());
    QCOMPARE(*snapshot.outputReference(), outputReference);
}

void NextPlatformQtJobWorkerTests::
    reportWorkerRejectsInvalidOutputReferencesAsFailures()
{
    const std::array<std::string, 4> invalidReferences{
        std::string{},
        std::string(" \t\n"),
        std::string("\xE3\x80\x80"),
        std::string(kReportJobMaxOutputReferenceLength + 1, 'x')
    };
    const OperationError expectedError{
        .code = ErrorCode::InvalidInput,
        .message = "Report output reference must be non-blank and bounded.",
        .recoverable = false
    };

    for (const std::string& invalidReference : invalidReferences)
    {
        QtReportJobWorker worker(
            [&invalidReference](const auto, const auto, auto&, const auto&)
            {
                return Result<std::string>::success(invalidReference);
            }
            );
        ReportJobCoordinator coordinator(worker);

        QVERIFY(coordinator.startJob(1));
        QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
        worker.waitForFinished();

        const auto taskResult = worker.lastResult();
        QVERIFY(taskResult.has_value());
        QVERIFY(!*taskResult);
        QCOMPARE(taskResult->error(), expectedError);
        QCOMPARE(coordinator.pendingEventCount(), std::size_t{2});

        const auto pump = coordinator.pump();
        QVERIFY(pump);
        QCOMPARE(pump.value(), std::size_t{2});
        const ReportJobSnapshot snapshot = coordinator.snapshot();
        QCOMPARE(snapshot.phase(), ReportJobPhase::Failed);
        QCOMPARE(snapshot.completedUnits(), ReportJobSnapshot::UnitCount{1});
        QVERIFY(snapshot.error().has_value());
        QCOMPARE(*snapshot.error(), expectedError);
        QVERIFY(!snapshot.outputReference().has_value());
    }
}

void NextPlatformQtJobWorkerTests::
    reportCancellationIsAcknowledgedAfterCooperativeWorkReturns()
{
    QMutex mutex;
    QWaitCondition condition;
    bool entered = false;
    bool released = false;
    const std::string outputReference = "reports/canceled-output.pdf";

    QtReportJobWorker worker(
        [&](const auto, const auto, auto&, const QtJobCancellationToken& token)
        {
            QMutexLocker locker(&mutex);
            entered = true;
            condition.wakeAll();
            while (!token.isCancellationRequested())
            {
                condition.wait(&mutex);
            }
            released = true;
            condition.wakeAll();
            return Result<std::string>::success(outputReference);
        }
        );
    ReportJobCoordinator coordinator(worker);

    QVERIFY(coordinator.startJob(5));
    QVERIFY(waitForFlag(mutex, condition, entered));
    QVERIFY(coordinator.requestCancellation());
    {
        QMutexLocker locker(&mutex);
        condition.wakeAll();
    }
    QVERIFY(waitForFlag(mutex, condition, released));
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();

    QVERIFY(coordinator.pump());
    const ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Canceled);
    QVERIFY(snapshot.cancellationRequested());
}

void NextPlatformQtJobWorkerTests::
    reportCancellationWinsOverInvalidOutputReference()
{
    QMutex mutex;
    QWaitCondition condition;
    bool entered = false;
    bool released = false;
    const std::string invalidOutputReference;

    QtReportJobWorker worker(
        [&](const auto, const auto, auto&, const QtJobCancellationToken& token)
        {
            QMutexLocker locker(&mutex);
            entered = true;
            condition.wakeAll();
            while (!token.isCancellationRequested())
            {
                condition.wait(&mutex);
            }
            released = true;
            condition.wakeAll();
            return Result<std::string>::success(invalidOutputReference);
        }
        );
    ReportJobCoordinator coordinator(worker);

    QVERIFY(coordinator.startJob(5));
    QVERIFY(waitForFlag(mutex, condition, entered));
    QVERIFY(coordinator.requestCancellation());
    {
        QMutexLocker locker(&mutex);
        condition.wakeAll();
    }
    QVERIFY(waitForFlag(mutex, condition, released));
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();

    const auto taskResult = worker.lastResult();
    QVERIFY(taskResult.has_value());
    QVERIFY(*taskResult);
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{2});

    const auto pump = coordinator.pump();
    QVERIFY(pump);
    QCOMPARE(pump.value(), std::size_t{2});
    const ReportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ReportJobPhase::Canceled);
    QVERIFY(snapshot.cancellationRequested());
    QCOMPARE(snapshot.completedUnits(), ReportJobSnapshot::UnitCount{5});
    QVERIFY(!snapshot.outputReference().has_value());
}

void NextPlatformQtJobWorkerTests::
    importCancellationIsAcknowledgedAfterCooperativeWorkReturns()
{
    QMutex mutex;
    QWaitCondition condition;
    bool entered = false;
    bool released = false;

    QtImportJobWorker worker(
        [&](const auto, const auto, auto&, const QtJobCancellationToken& token)
        {
            QMutexLocker locker(&mutex);
            entered = true;
            condition.wakeAll();
            while (!token.isCancellationRequested())
            {
                condition.wait(&mutex);
            }
            released = true;
            condition.wakeAll();
            return Result<void>::success();
        }
        );
    ImportJobCoordinator coordinator(worker);

    QVERIFY(coordinator.startJob(5));
    QVERIFY(waitForFlag(mutex, condition, entered));
    QVERIFY(coordinator.requestCancellation());
    {
        QMutexLocker locker(&mutex);
        condition.wakeAll();
    }
    QVERIFY(waitForFlag(mutex, condition, released));
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();

    QVERIFY(coordinator.pump());
    const ImportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ImportJobPhase::Canceled);
    QVERIFY(snapshot.cancellationRequested());
}

void NextPlatformQtJobWorkerTests::taskFailureAndExceptionProduceFailedEvents()
{
    enum class Behavior
    {
        ReturnFailure,
        ThrowException
    };
    Behavior behavior = Behavior::ReturnFailure;
    const OperationError expected = taskError("Import task failed.");

    QtImportJobWorker worker(
        [&](const auto, const auto, auto&, const auto&) -> Result<void>
        {
            if (behavior == Behavior::ThrowException)
            {
                throw std::runtime_error("worker failure");
            }
            return Result<void>::failure(expected);
        }
        );
    ImportJobCoordinator coordinator(worker);

    QVERIFY(coordinator.startJob(1));
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();
    QVERIFY(coordinator.pump());
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Failed);
    QVERIFY(coordinator.snapshot().error().has_value());
    QCOMPARE(*coordinator.snapshot().error(), expected);

    behavior = Behavior::ThrowException;
    QVERIFY(coordinator.startJob(1));
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();
    QVERIFY(coordinator.pump());
    const ImportJobSnapshot snapshot = coordinator.snapshot();
    QCOMPARE(snapshot.phase(), ImportJobPhase::Failed);
    QVERIFY(snapshot.error().has_value());
    QCOMPARE(snapshot.error()->code, ErrorCode::Technical);
    QCOMPARE(
        snapshot.error()->message,
        std::string("Import job worker callback raised an exception.")
        );
}

void NextPlatformQtJobWorkerTests::
    zeroCapacityQueueMakesLastResultReportPostFailure()
{
    QtImportJobWorker worker(
        [](const auto, const auto, auto&, const auto&)
        {
            return Result<void>::success();
        }
        );
    ImportJobCoordinator coordinator(worker, 0);

    QVERIFY(coordinator.startJob(1));
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();

    const auto result = worker.lastResult();
    QVERIFY(result.has_value());
    QVERIFY(!*result);
    QCOMPARE(result->error().code, ErrorCode::Conflict);
    QCOMPARE(
        result->error().message,
        std::string("Import job event queue capacity has been reached.")
        );
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{0});
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Running);
}

void NextPlatformQtJobWorkerTests::
    reportZeroCapacityQueueMakesLastResultReportPostFailure()
{
    const OperationError callbackError = taskError("Report callback failure.");
    bool workExecuted = false;
    QtReportJobWorker worker(
        [&](const auto, const auto, auto&, const auto&)
        {
            workExecuted = true;
            return Result<std::string>::failure(callbackError);
        }
        );
    ReportJobCoordinator coordinator(worker, 0);

    QVERIFY(coordinator.startJob(1));
    QCOMPARE(coordinator.eventQueueCapacity(), std::size_t{0});
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();

    QVERIFY(workExecuted);
    const auto result = worker.lastResult();
    QVERIFY(result.has_value());
    QVERIFY(!*result);
    QCOMPARE(result->error().code, ErrorCode::Conflict);
    QCOMPARE(
        result->error().message,
        std::string("Report job event queue capacity has been reached.")
        );
    QVERIFY(result->error() != callbackError);
    QCOMPARE(coordinator.pendingEventCount(), std::size_t{0});
    const auto pump = coordinator.pump();
    QVERIFY(pump);
    QCOMPARE(pump.value(), std::size_t{0});
    QCOMPARE(coordinator.snapshot().phase(), ReportJobPhase::Running);
}

void NextPlatformQtJobWorkerTests::
    duplicateActiveStartIsRejectedAndFinishedWorkerCanRestart()
{
    QMutex mutex;
    QWaitCondition condition;
    bool entered = false;
    bool releaseFirst = false;
    int invocationCount = 0;
    ImportJobEventQueue queue;
    QtImportJobWorker worker(
        [&](const auto, const auto, auto&, const auto&)
        {
            QMutexLocker locker(&mutex);
            ++invocationCount;
            if (invocationCount == 1)
            {
                entered = true;
                condition.wakeAll();
                while (!releaseFirst)
                {
                    condition.wait(&mutex);
                }
            }
            return Result<void>::success();
        }
        );

    QVERIFY(worker.start(1, ImportJobGeneration{11}, queue));
    QVERIFY(waitForFlag(mutex, condition, entered));
    const auto duplicate = worker.start(1, ImportJobGeneration{12}, queue);
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, ErrorCode::Conflict);

    {
        QMutexLocker locker(&mutex);
        releaseFirst = true;
        condition.wakeAll();
    }
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();

    QVERIFY(worker.start(2, ImportJobGeneration{12}, queue));
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();
    QCOMPARE(invocationCount, 2);
    const auto events = queue.drain();
    QCOMPARE(events.size(), std::size_t{4});
    QVERIFY(events[0].generation == ImportJobGeneration{11});
    QVERIFY(events[2].generation == ImportJobGeneration{12});
}

void NextPlatformQtJobWorkerTests::
    completedJobRemainsCompletedAfterLaterCancellationRequest()
{
    QtImportJobWorker worker(
        [](const auto, const auto, auto&, const auto&)
        {
            return Result<void>::success();
        }
        );
    ImportJobCoordinator coordinator(worker);

    QVERIFY(coordinator.startJob(1));
    QTRY_VERIFY_WITH_TIMEOUT(!worker.isRunning(), kTimeoutMs);
    worker.waitForFinished();
    QVERIFY(coordinator.pump());
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Completed);

    const auto cancellation = coordinator.requestCancellation();
    QVERIFY(!cancellation);
    QCOMPARE(cancellation.error().code, ErrorCode::Conflict);
    QCOMPARE(coordinator.snapshot().phase(), ImportJobPhase::Completed);
    QVERIFY(!coordinator.snapshot().cancellationRequested());
}

void NextPlatformQtJobWorkerTests::
    destructorRequestsCancellationAndJoinsCooperativeWork()
{
    QMutex mutex;
    QWaitCondition condition;
    bool entered = false;
    bool exited = false;
    ImportJobEventQueue queue;

    {
        QtImportJobWorker worker(
            [&](const auto, const auto, auto&, const QtJobCancellationToken& token)
            {
                QMutexLocker locker(&mutex);
                entered = true;
                condition.wakeAll();
                while (!token.isCancellationRequested())
                {
                    condition.wait(&mutex, 10);
                }
                exited = true;
                condition.wakeAll();
                return Result<void>::success();
            }
            );

        QVERIFY(worker.start(1, ImportJobGeneration{1}, queue));
        QVERIFY(waitForFlag(mutex, condition, entered));
    }

    QVERIFY(exited);
    const auto events = queue.drain();
    QCOMPARE(events.size(), std::size_t{2});
    QCOMPARE(events[0].kind, ImportJobEventKind::Progress);
    QCOMPARE(events[1].kind, ImportJobEventKind::CancellationAcknowledged);
}

QTEST_GUILESS_MAIN(NextPlatformQtJobWorkerTests)

#include "next_platform_qt_job_worker_tests.moc"
