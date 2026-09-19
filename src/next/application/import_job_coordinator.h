#pragma once

#include "next/application/import_job_state.h"

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

using ImportJobGeneration = std::uint64_t;
using ImportJobSessionToken = ImportJobGeneration;

// The default is deliberately small. Callers can choose a smaller bound for
// tests or an adapter with a more restrictive burst profile; the queue never
// reserves this capacity up front.
inline constexpr std::size_t kImportJobEventQueueCapacity = 64;

enum class ImportJobEventKind
{
    Progress,
    CancellationAcknowledged,
    Completed,
    Failed
};

using ImportJobEventType = ImportJobEventKind;

// A worker event contains only lifecycle data. The generation prevents an
// event retained by an earlier worker from advancing a later job.
struct ImportJobEvent final
{
    using Generation = ImportJobGeneration;
    using ItemCount = ImportJobSnapshot::ItemCount;

    ImportJobEventKind kind = ImportJobEventKind::Progress;
    Generation generation = 0;
    ItemCount completedItems = 0;
    std::optional<Domain::OperationError> error;

    [[nodiscard]] static ImportJobEvent progress(
        const Generation generation,
        const ItemCount completedItems
        ) noexcept
    {
        return ImportJobEvent{
            .kind = ImportJobEventKind::Progress,
            .generation = generation,
            .completedItems = completedItems
        };
    }

    [[nodiscard]] static ImportJobEvent cancellationAcknowledged(
        const Generation generation
        ) noexcept
    {
        return ImportJobEvent{
            .kind = ImportJobEventKind::CancellationAcknowledged,
            .generation = generation
        };
    }

    [[nodiscard]] static ImportJobEvent completed(
        const Generation generation
        ) noexcept
    {
        return ImportJobEvent{
            .kind = ImportJobEventKind::Completed,
            .generation = generation
        };
    }

    [[nodiscard]] static ImportJobEvent failed(
        const Generation generation,
        Domain::OperationError error
        )
    {
        return ImportJobEvent{
            .kind = ImportJobEventKind::Failed,
            .generation = generation,
            .error = std::move(error)
        };
    }

    friend bool operator==(
        const ImportJobEvent&,
        const ImportJobEvent&
        ) = default;
};

// The only object a worker receives for publishing lifecycle events. It has
// no reference to ImportJobState or any other application-owned state.
class ImportJobEventSink
{
public:
    virtual ~ImportJobEventSink() = default;

    [[nodiscard]] virtual Domain::Result<void> post(
        ImportJobEvent event
        ) = 0;
};

// Thread-safe, bounded FIFO handoff from a worker to the application owner.
// A capacity of zero is valid and rejects every post without allocating.
class ImportJobEventQueue final : public ImportJobEventSink
{
public:
    explicit ImportJobEventQueue(
        const std::size_t capacity = kImportJobEventQueueCapacity
        ) noexcept
        : m_capacity(capacity)
    {
    }

    [[nodiscard]] Domain::Result<void> post(
        ImportJobEvent event
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
    [[nodiscard]] std::vector<ImportJobEvent> drain()
    {
        std::lock_guard lock(m_mutex);
        std::vector<ImportJobEvent> events;
        events.reserve(m_events.size());
        while (!m_events.empty())
        {
            events.push_back(std::move(m_events.front()));
            m_events.pop_front();
        }
        return events;
    }

    [[nodiscard]] std::optional<ImportJobEvent> tryPop()
    {
        std::lock_guard lock(m_mutex);
        if (m_events.empty())
        {
            return std::nullopt;
        }

        ImportJobEvent event = std::move(m_events.front());
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
            .message = "Import job event queue capacity has been reached.",
            .recoverable = true
        };
    }

    const std::size_t m_capacity;
    mutable std::mutex m_mutex;
    std::deque<ImportJobEvent> m_events;
};

// Adapter-neutral worker boundary. A worker gets a generation and sink, but
// never gets the state owner or a mutable snapshot.
class ImportJobWorkerPort
{
public:
    using Generation = ImportJobGeneration;
    using ItemCount = ImportJobSnapshot::ItemCount;

    virtual ~ImportJobWorkerPort() = default;

    [[nodiscard]] virtual Domain::Result<void> start(
        ItemCount totalItems,
        Generation generation,
        ImportJobEventSink& eventSink
        ) = 0;

    [[nodiscard]] virtual Domain::Result<void> requestCancellation() = 0;
};

// Synchronous application owner for one import-job lifecycle. State changes
// caused by worker events happen only inside pump(); startJob() is the only
// explicit reset boundary and clears events retained from a previous job.
class ImportJobCoordinator final
{
public:
    using Generation = ImportJobGeneration;
    using SessionToken = ImportJobSessionToken;
    using ItemCount = ImportJobSnapshot::ItemCount;
    using PumpedEventCount = std::size_t;

    explicit ImportJobCoordinator(
        ImportJobWorkerPort& worker,
        const std::size_t queueCapacity = kImportJobEventQueueCapacity
        ) noexcept
        : m_worker(worker),
          m_eventQueue(queueCapacity)
    {
    }

    [[nodiscard]] Domain::Result<Generation> startJob(
        const ItemCount totalItems
        )
    {
        if (m_generation == std::numeric_limits<Generation>::max())
        {
            return Domain::Result<Generation>::failure(
                generationExhausted()
                );
        }

        const auto stateResult = m_state.start(totalItems);
        if (!stateResult)
        {
            return Domain::Result<Generation>::failure(stateResult.error());
        }

        // Incrementing and clearing happen together at the successful start
        // boundary. A rejected duplicate start leaves both state and queue
        // untouched.
        ++m_generation;
        m_eventQueue.clear();

        const auto workerResult = invokeWorkerStart(totalItems, m_generation);
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
        const ItemCount totalItems
        )
    {
        return startJob(totalItems);
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
        const std::vector<ImportJobEvent> events = m_eventQueue.drain();
        for (const ImportJobEvent& event : events)
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

    [[nodiscard]] ImportJobSnapshot snapshot() const
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
        const ItemCount totalItems,
        const Generation generation
        )
    {
        try
        {
            return m_worker.start(totalItems, generation, m_eventQueue);
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(
                workerException(
                    "Import worker start raised an exception."
                    )
                );
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                workerException(
                    "Import worker start raised an unknown exception."
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
                    "Import worker cancellation raised an exception."
                    )
                );
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                workerException(
                    "Import worker cancellation raised an unknown exception."
                    )
                );
        }
    }

    void apply(
        const ImportJobEvent& event
        )
    {
        switch (event.kind)
        {
        case ImportJobEventKind::Progress:
            (void)m_state.reportProgress(event.completedItems);
            break;
        case ImportJobEventKind::CancellationAcknowledged:
            (void)m_state.acknowledgeCancellation();
            break;
        case ImportJobEventKind::Completed:
            (void)m_state.complete();
            break;
        case ImportJobEventKind::Failed:
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
            .message = "Import job generation is exhausted.",
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
            .message = "Import failure event must contain a structured error.",
            .recoverable = false
        };
    }

    ImportJobWorkerPort& m_worker;
    ImportJobState m_state;
    ImportJobEventQueue m_eventQueue;
    Generation m_generation = 0;
};

} // namespace ClassMngr::Next::Application
