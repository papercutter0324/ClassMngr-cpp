#pragma once

#include "next/application/report_job_state.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

using ReportJobGeneration = std::uint64_t;
using ReportJobSessionToken = ReportJobGeneration;

// The default is deliberately small. Callers can choose a smaller bound for
// tests or an adapter with a more restrictive burst profile; the queue never
// reserves this capacity up front.
inline constexpr std::size_t kReportJobEventQueueCapacity = 64;

enum class ReportJobEventKind
{
    Progress,
    CancellationAcknowledged,
    Completed,
    Failed
};

using ReportJobEventType = ReportJobEventKind;

// A worker event contains only lifecycle data. The generation prevents an
// event retained by an earlier worker from advancing a later job.
struct ReportJobEvent final
{
    using Generation = ReportJobGeneration;
    using UnitCount = ReportJobSnapshot::UnitCount;
    using OutputReference = ReportJobSnapshot::OutputReference;

    ReportJobEventKind kind = ReportJobEventKind::Progress;
    Generation generation = 0;
    UnitCount completedUnits = 0;
    OutputReference outputReference;
    std::optional<Domain::OperationError> error;

    [[nodiscard]] static ReportJobEvent progress(
        const Generation generation,
        const UnitCount completedUnits
        ) noexcept
    {
        return ReportJobEvent{
            .kind = ReportJobEventKind::Progress,
            .generation = generation,
            .completedUnits = completedUnits
        };
    }

    [[nodiscard]] static ReportJobEvent cancellationAcknowledged(
        const Generation generation
        ) noexcept
    {
        return ReportJobEvent{
            .kind = ReportJobEventKind::CancellationAcknowledged,
            .generation = generation
        };
    }

    [[nodiscard]] static ReportJobEvent completed(
        const Generation generation,
        OutputReference outputReference
        )
    {
        return ReportJobEvent{
            .kind = ReportJobEventKind::Completed,
            .generation = generation,
            .outputReference = std::move(outputReference)
        };
    }

    [[nodiscard]] static ReportJobEvent failed(
        const Generation generation,
        Domain::OperationError error
        )
    {
        return ReportJobEvent{
            .kind = ReportJobEventKind::Failed,
            .generation = generation,
            .error = std::move(error)
        };
    }

    friend bool operator==(
        const ReportJobEvent&,
        const ReportJobEvent&
        ) = default;
};

// The only object a worker receives for publishing lifecycle events. It has
// no reference to ReportJobState or any other application-owned state.
class ReportJobEventSink
{
public:
    virtual ~ReportJobEventSink() = default;

    [[nodiscard]] virtual Domain::Result<void> post(
        ReportJobEvent event
        ) = 0;
};

// Thread-safe, bounded FIFO handoff from a worker to the application owner.
// A capacity of zero is valid and rejects every post without allocating.
class ReportJobEventQueue final : public ReportJobEventSink
{
public:
    explicit ReportJobEventQueue(
        const std::size_t capacity = kReportJobEventQueueCapacity
        ) noexcept
        : m_capacity(capacity)
    {
    }

    [[nodiscard]] Domain::Result<void> post(
        ReportJobEvent event
        ) override
    {
        std::lock_guard lock(m_mutex);
        if (m_events.size() >= m_capacity)
        {
            return Domain::Result<void>::failure(queueOverflow());
        }

        m_events.push_back(std::move(event));
        return Domain::Result<void>::success();
    }

    // Moves the currently queued batch in FIFO order. Events posted after the
    // lock is released remain for the next drain.
    [[nodiscard]] std::vector<ReportJobEvent> drain()
    {
        std::lock_guard lock(m_mutex);
        std::vector<ReportJobEvent> events;
        events.reserve(m_events.size());
        while (!m_events.empty())
        {
            events.push_back(std::move(m_events.front()));
            m_events.pop_front();
        }
        return events;
    }

    [[nodiscard]] std::optional<ReportJobEvent> tryPop()
    {
        std::lock_guard lock(m_mutex);
        if (m_events.empty())
        {
            return std::nullopt;
        }

        ReportJobEvent event = std::move(m_events.front());
        m_events.pop_front();
        return event;
    }

    void clear() noexcept
    {
        std::lock_guard lock(m_mutex);
        m_events.clear();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        std::lock_guard lock(m_mutex);
        return m_events.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept
    {
        return m_capacity;
    }

private:
    [[nodiscard]] static Domain::OperationError queueOverflow()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = "Report job event queue capacity has been reached.",
            .recoverable = true
        };
    }

    const std::size_t m_capacity;
    mutable std::mutex m_mutex;
    std::deque<ReportJobEvent> m_events;
};

// Adapter-neutral worker boundary. A worker gets a generation and sink, but
// never gets the state owner or a mutable snapshot.
class ReportJobWorkerPort
{
public:
    using Generation = ReportJobGeneration;
    using UnitCount = ReportJobSnapshot::UnitCount;

    virtual ~ReportJobWorkerPort() = default;

    [[nodiscard]] virtual Domain::Result<void> start(
        UnitCount totalUnits,
        Generation generation,
        ReportJobEventSink& eventSink
        ) = 0;

    [[nodiscard]] virtual Domain::Result<void> requestCancellation() = 0;
};

// Synchronous application owner for one report-job lifecycle. State changes
// caused by worker events happen only inside pump(); startJob() is the only
// explicit reset boundary and clears events retained from a previous job.
class ReportJobCoordinator final
{
public:
    using Generation = ReportJobGeneration;
    using SessionToken = ReportJobSessionToken;
    using UnitCount = ReportJobSnapshot::UnitCount;
    using PumpedEventCount = std::size_t;

    explicit ReportJobCoordinator(
        ReportJobWorkerPort& worker,
        const std::size_t queueCapacity = kReportJobEventQueueCapacity
        ) noexcept
        : m_worker(worker),
          m_eventQueue(queueCapacity)
    {
    }

    [[nodiscard]] Domain::Result<Generation> startJob(
        const UnitCount totalUnits
        )
    {
        if (m_generation == std::numeric_limits<Generation>::max())
        {
            return Domain::Result<Generation>::failure(
                generationExhausted()
                );
        }

        const auto stateResult = m_state.start(totalUnits);
        if (!stateResult)
        {
            return Domain::Result<Generation>::failure(stateResult.error());
        }

        // Incrementing and clearing happen together at the successful start
        // boundary. A rejected duplicate start leaves both state and queue
        // untouched.
        ++m_generation;
        m_eventQueue.clear();

        const auto workerResult = invokeWorkerStart(totalUnits, m_generation);
        if (!workerResult)
        {
            m_eventQueue.clear();
            const auto failureResult = m_state.fail(workerResult.error());
            if (!failureResult)
            {
                return Domain::Result<Generation>::failure(
                    failureResult.error()
                    );
            }

            return Domain::Result<Generation>::failure(workerResult.error());
        }

        return Domain::Result<Generation>::success(m_generation);
    }

    [[nodiscard]] Domain::Result<Generation> start(
        const UnitCount totalUnits
        )
    {
        return startJob(totalUnits);
    }

    [[nodiscard]] Domain::Result<void> requestCancellation()
    {
        const auto stateResult = m_state.requestCancellation();
        if (!stateResult)
        {
            return stateResult;
        }

        // The state records the request before the adapter command is sent,
        // so a worker-side failure still leaves an explicit requested state.
        return invokeWorkerCancellation();
    }

    // Drains one stable FIFO batch. Stale-generation and terminal-state
    // events are consumed but cannot mutate the current snapshot.
    [[nodiscard]] Domain::Result<PumpedEventCount> pump()
    {
        const std::vector<ReportJobEvent> events = m_eventQueue.drain();
        for (const ReportJobEvent& event : events)
        {
            if (event.generation != m_generation)
            {
                continue;
            }

            apply(event);
        }

        return Domain::Result<PumpedEventCount>::success(events.size());
    }

    [[nodiscard]] Domain::Result<PumpedEventCount> pumpEvents()
    {
        return pump();
    }

    [[nodiscard]] Domain::Result<PumpedEventCount> drain()
    {
        return pump();
    }

    [[nodiscard]] ReportJobSnapshot snapshot() const
    {
        return m_state.snapshot();
    }

    [[nodiscard]] Generation generation() const noexcept
    {
        return m_generation;
    }

    [[nodiscard]] Generation currentGeneration() const noexcept
    {
        return m_generation;
    }

    [[nodiscard]] std::size_t pendingEventCount() const noexcept
    {
        return m_eventQueue.size();
    }

    [[nodiscard]] std::size_t eventQueueCapacity() const noexcept
    {
        return m_eventQueue.capacity();
    }

private:
    [[nodiscard]] Domain::Result<void> invokeWorkerStart(
        const UnitCount totalUnits,
        const Generation generation
        )
    {
        try
        {
            return m_worker.start(totalUnits, generation, m_eventQueue);
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(
                workerException(
                    "Report worker start raised an exception."
                    )
                );
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                workerException(
                    "Report worker start raised an unknown exception."
                    )
                );
        }
    }

    [[nodiscard]] Domain::Result<void> invokeWorkerCancellation()
    {
        try
        {
            return m_worker.requestCancellation();
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(
                workerException(
                    "Report worker cancellation raised an exception."
                    )
                );
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                workerException(
                    "Report worker cancellation raised an unknown exception."
                    )
                );
        }
    }

    void apply(
        const ReportJobEvent& event
        )
    {
        switch (event.kind)
        {
        case ReportJobEventKind::Progress:
            (void)m_state.reportProgress(event.completedUnits);
            break;
        case ReportJobEventKind::CancellationAcknowledged:
            (void)m_state.acknowledgeCancellation();
            break;
        case ReportJobEventKind::Completed:
            (void)m_state.complete(event.outputReference);
            break;
        case ReportJobEventKind::Failed:
            if (event.error.has_value())
            {
                (void)m_state.fail(*event.error);
            }
            else
            {
                (void)m_state.fail(missingFailureError());
            }
            break;
        }
    }

    [[nodiscard]] static Domain::OperationError generationExhausted()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = "Report job generation is exhausted.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError workerException(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError missingFailureError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Report failure event must contain a structured error.",
            .recoverable = false
        };
    }

    ReportJobWorkerPort& m_worker;
    ReportJobState m_state;
    ReportJobEventQueue m_eventQueue;
    Generation m_generation = 0;
};

} // namespace ClassMngr::Next::Application
