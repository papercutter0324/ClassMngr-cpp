#pragma once

#include "next/domain/operation_result.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class ImportJobPhase
{
    Idle,
    Running,
    Completed,
    Failed,
    Canceled
};

// A copyable value projection of one import operation. The application owner
// publishes copies of this snapshot to consumers; no worker owns or mutates
// the projection directly.
class ImportJobSnapshot final
{
public:
    using ItemCount = std::size_t;

    ImportJobSnapshot() = default;

    [[nodiscard]] ImportJobPhase phase() const noexcept
    {
        return m_phase;
    }

    [[nodiscard]] ItemCount totalItems() const noexcept
    {
        return m_totalItems;
    }

    [[nodiscard]] ItemCount completedItems() const noexcept
    {
        return m_completedItems;
    }

    [[nodiscard]] bool cancellationRequested() const noexcept
    {
        return m_cancellationRequested;
    }

    [[nodiscard]] const std::optional<Domain::OperationError>& error() const
        noexcept
    {
        return m_error;
    }

    friend bool operator==(
        const ImportJobSnapshot&,
        const ImportJobSnapshot&
        ) = default;

private:
    friend class ImportJobState;

    ImportJobPhase m_phase = ImportJobPhase::Idle;
    ItemCount m_totalItems = 0;
    ItemCount m_completedItems = 0;
    bool m_cancellationRequested = false;
    std::optional<Domain::OperationError> m_error;
};

// Owns one import-job lifecycle. The application owner serializes worker
// events through this object; worker code must not mutate it directly.
class ImportJobState final
{
public:
    using ItemCount = ImportJobSnapshot::ItemCount;

    ImportJobState() = default;

    [[nodiscard]] ImportJobSnapshot snapshot() const
    {
        return m_snapshot;
    }

    [[nodiscard]] Domain::Result<void> start(
        ItemCount totalItems
        )
    {
        if (m_snapshot.phase() == ImportJobPhase::Running)
        {
            return Domain::Result<void>::failure(
                conflict("Cannot start an import while another import is running.")
                );
        }

        m_snapshot = ImportJobSnapshot{};
        m_snapshot.m_phase = ImportJobPhase::Running;
        m_snapshot.m_totalItems = totalItems;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> reportProgress(
        ItemCount completedItems
        )
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict("Import progress can only be reported while running.")
                );
        }

        if (completedItems > m_snapshot.m_totalItems)
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Import progress cannot exceed the total item count."
                    )
                );
        }

        m_snapshot.m_completedItems = completedItems;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> requestCancellation()
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Import cancellation can only be requested while running."
                    )
                );
        }

        m_snapshot.m_cancellationRequested = true;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> acknowledgeCancellation()
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Import cancellation can only be acknowledged while running."
                    )
                );
        }

        m_snapshot.m_phase = ImportJobPhase::Canceled;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> cancel()
    {
        return acknowledgeCancellation();
    }

    [[nodiscard]] Domain::Result<void> complete()
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict("Import can only be completed while running.")
                );
        }

        if (m_snapshot.m_completedItems != m_snapshot.m_totalItems)
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Import cannot be completed before all items are complete."
                    )
                );
        }

        // A cancellation request is advisory until the worker acknowledges it;
        // final progress reported first allows completion to win the race.
        m_snapshot.m_phase = ImportJobPhase::Completed;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> fail(
        Domain::OperationError error
        )
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict("Import can only fail while running.")
                );
        }

        m_snapshot.m_phase = ImportJobPhase::Failed;
        m_snapshot.m_error = std::move(error);
        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_snapshot.phase() == ImportJobPhase::Running;
    }

    [[nodiscard]] static Domain::OperationError invalidInput(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError conflict(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = message,
            .recoverable = true
        };
    }

    ImportJobSnapshot m_snapshot;
};

}
