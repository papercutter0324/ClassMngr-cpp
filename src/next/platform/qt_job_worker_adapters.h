#pragma once

#include "next/application/import_job_coordinator.h"
#include "next/application/report_job_coordinator.h"
#include "next/platform/qt_job_worker_lifetime.h"

#include <exception>
#include <functional>
#include <optional>
#include <utility>

namespace ClassMngr::Next::Platform
{

// The caller must keep the supplied event sink alive until this adapter is
// destroyed or its worker has been joined. The adapter never captures a
// coordinator or application state; it emits value events through the sink.
// Work and sink references remain retained until QtJobWorkerLifetime joins.
class QtImportJobWorker final : public Application::ImportJobWorkerPort
{
public:
    using Generation = Application::ImportJobWorkerPort::Generation;
    using ItemCount = Application::ImportJobWorkerPort::ItemCount;
    using Work = std::function<Domain::Result<void>(
        ItemCount,
        Generation,
        Application::ImportJobEventSink&,
        const QtJobCancellationToken&
        )>;

    explicit QtImportJobWorker(
        Work work
        )
        : m_work(std::move(work))
    {
    }

    ~QtImportJobWorker() override = default;

    QtImportJobWorker(const QtImportJobWorker&) = delete;
    QtImportJobWorker& operator=(const QtImportJobWorker&) = delete;
    QtImportJobWorker(QtImportJobWorker&&) = delete;
    QtImportJobWorker& operator=(QtImportJobWorker&&) = delete;

    [[nodiscard]] Domain::Result<void> start(
        const ItemCount totalItems,
        const Generation generation,
        Application::ImportJobEventSink& eventSink
        ) override
    {
        if (!m_work)
        {
            return Domain::Result<void>::failure(emptyWorkError());
        }

        return m_lifetime.start(
            [this, totalItems, generation, &eventSink](
                const QtJobCancellationToken& cancellationToken
                ) -> Domain::Result<void>
            {
                Domain::Result<void> workResult = invokeWork(
                    totalItems,
                    generation,
                    eventSink,
                    cancellationToken
                    );
                if (!workResult)
                {
                    const auto postResult = postEvent(
                        eventSink,
                        Application::ImportJobEvent::failed(
                            generation,
                            workResult.error()
                            )
                        );
                    return postResult ? workResult : postResult;
                }

                auto postResult = postEvent(
                    eventSink,
                    Application::ImportJobEvent::progress(
                        generation,
                        totalItems
                        )
                    );
                if (!postResult)
                {
                    return postResult;
                }

                if (cancellationToken.isCancellationRequested())
                {
                    return postEvent(
                        eventSink,
                        Application::ImportJobEvent::cancellationAcknowledged(
                            generation
                            )
                        );
                }

                return postEvent(
                    eventSink,
                    Application::ImportJobEvent::completed(generation)
                    );
            }
            );
    }

    [[nodiscard]] Domain::Result<void> requestCancellation() override
    {
        return m_lifetime.requestCancellation();
    }

    [[nodiscard]] bool isRunning() const
    {
        return m_lifetime.isRunning();
    }

    void waitForFinished()
    {
        m_lifetime.waitForFinished();
    }

    [[nodiscard]] std::optional<Domain::Result<void>> lastResult() const
    {
        return m_lifetime.lastResult();
    }

private:
    [[nodiscard]] Domain::Result<void> invokeWork(
        const ItemCount totalItems,
        const Generation generation,
        Application::ImportJobEventSink& eventSink,
        const QtJobCancellationToken& cancellationToken
        )
    {
        try
        {
            return m_work(
                totalItems,
                generation,
                eventSink,
                cancellationToken
                );
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(workExceptionError());
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                unknownWorkExceptionError()
                );
        }
    }

    [[nodiscard]] static Domain::Result<void> postEvent(
        Application::ImportJobEventSink& eventSink,
        Application::ImportJobEvent event
        ) noexcept
    {
        try
        {
            return eventSink.post(std::move(event));
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(postExceptionError());
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                unknownPostExceptionError()
                );
        }
    }

    [[nodiscard]] static Domain::OperationError emptyWorkError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Import job worker callback must not be empty.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError workExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Import job worker callback raised an exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError unknownWorkExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Import job worker callback raised an unknown exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError postExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Import job event publication raised an exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError unknownPostExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Import job event publication raised an unknown exception.",
            .recoverable = false
        };
    }

    // Declaration order is deliberate: lifetime is destroyed first and joins
    // before the callback captured by its task can be destroyed.
    Work m_work;
    QtJobWorkerLifetime m_lifetime;
};

// The caller must keep the supplied event sink alive until this adapter is
// destroyed or its worker has been joined. The adapter never captures a
// coordinator or application state; it emits value events through the sink.
// Work and sink references remain retained until QtJobWorkerLifetime joins.
class QtReportJobWorker final : public Application::ReportJobWorkerPort
{
public:
    using Generation = Application::ReportJobWorkerPort::Generation;
    using UnitCount = Application::ReportJobWorkerPort::UnitCount;
    using OutputReference = Application::ReportJobSnapshot::OutputReference;
    using Work = std::function<Domain::Result<OutputReference>(
        UnitCount,
        Generation,
        Application::ReportJobEventSink&,
        const QtJobCancellationToken&
        )>;

    explicit QtReportJobWorker(
        Work work
        )
        : m_work(std::move(work))
    {
    }

    ~QtReportJobWorker() override = default;

    QtReportJobWorker(const QtReportJobWorker&) = delete;
    QtReportJobWorker& operator=(const QtReportJobWorker&) = delete;
    QtReportJobWorker(QtReportJobWorker&&) = delete;
    QtReportJobWorker& operator=(QtReportJobWorker&&) = delete;

    [[nodiscard]] Domain::Result<void> start(
        const UnitCount totalUnits,
        const Generation generation,
        Application::ReportJobEventSink& eventSink
        ) override
    {
        if (!m_work)
        {
            return Domain::Result<void>::failure(emptyWorkError());
        }

        return m_lifetime.start(
            [this, totalUnits, generation, &eventSink](
                const QtJobCancellationToken& cancellationToken
                ) -> Domain::Result<void>
            {
                Domain::Result<OutputReference> workResult = invokeWork(
                    totalUnits,
                    generation,
                    eventSink,
                    cancellationToken
                    );
                if (!workResult)
                {
                    const auto postResult = postEvent(
                        eventSink,
                        Application::ReportJobEvent::failed(
                            generation,
                            workResult.error()
                            )
                        );
                    return postResult
                        ? Domain::Result<void>::failure(workResult.error())
                        : postResult;
                }

                auto postResult = postEvent(
                    eventSink,
                    Application::ReportJobEvent::progress(
                        generation,
                        totalUnits
                        )
                    );
                if (!postResult)
                {
                    return postResult;
                }

                if (cancellationToken.isCancellationRequested())
                {
                    return postEvent(
                        eventSink,
                        Application::ReportJobEvent::cancellationAcknowledged(
                            generation
                            )
                        );
                }

                return postEvent(
                    eventSink,
                    Application::ReportJobEvent::completed(
                        generation,
                        std::move(workResult.value())
                        )
                    );
            }
            );
    }

    [[nodiscard]] Domain::Result<void> requestCancellation() override
    {
        return m_lifetime.requestCancellation();
    }

    [[nodiscard]] bool isRunning() const
    {
        return m_lifetime.isRunning();
    }

    void waitForFinished()
    {
        m_lifetime.waitForFinished();
    }

    [[nodiscard]] std::optional<Domain::Result<void>> lastResult() const
    {
        return m_lifetime.lastResult();
    }

private:
    [[nodiscard]] Domain::Result<OutputReference> invokeWork(
        const UnitCount totalUnits,
        const Generation generation,
        Application::ReportJobEventSink& eventSink,
        const QtJobCancellationToken& cancellationToken
        )
    {
        try
        {
            return m_work(
                totalUnits,
                generation,
                eventSink,
                cancellationToken
                );
        }
        catch (const std::exception&)
        {
            return Domain::Result<OutputReference>::failure(
                workExceptionError()
                );
        }
        catch (...)
        {
            return Domain::Result<OutputReference>::failure(
                unknownWorkExceptionError()
                );
        }
    }

    [[nodiscard]] static Domain::Result<void> postEvent(
        Application::ReportJobEventSink& eventSink,
        Application::ReportJobEvent event
        ) noexcept
    {
        try
        {
            return eventSink.post(std::move(event));
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(postExceptionError());
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                unknownPostExceptionError()
                );
        }
    }

    [[nodiscard]] static Domain::OperationError emptyWorkError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Report job worker callback must not be empty.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError workExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Report job worker callback raised an exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError unknownWorkExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Report job worker callback raised an unknown exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError postExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Report job event publication raised an exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError unknownPostExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Report job event publication raised an unknown exception.",
            .recoverable = false
        };
    }

    // Declaration order is deliberate: lifetime is destroyed first and joins
    // before the callback captured by its task can be destroyed.
    Work m_work;
    QtJobWorkerLifetime m_lifetime;
};

} // namespace ClassMngr::Next::Platform
