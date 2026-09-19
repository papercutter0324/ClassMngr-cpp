#pragma once

#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kReportJobMaxOutputReferenceLength = 4'096;

enum class ReportJobPhase
{
    Idle,
    Running,
    Completed,
    Failed,
    Canceled
};

// A copyable value projection of one report, PDF, or export operation. The
// bounded output reference is populated only after successful completion.
class ReportJobSnapshot final
{
public:
    using UnitCount = std::size_t;
    using OutputReference = std::string;

    ReportJobSnapshot() = default;

    [[nodiscard]] ReportJobPhase phase() const noexcept
    {
        return m_phase;
    }

    [[nodiscard]] UnitCount totalUnits() const noexcept
    {
        return m_totalUnits;
    }

    [[nodiscard]] UnitCount completedUnits() const noexcept
    {
        return m_completedUnits;
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

    [[nodiscard]] const std::optional<OutputReference>& outputReference() const
        noexcept
    {
        return m_outputReference;
    }

    friend bool operator==(
        const ReportJobSnapshot&,
        const ReportJobSnapshot&
        ) = default;

private:
    friend class ReportJobState;

    ReportJobPhase m_phase = ReportJobPhase::Idle;
    UnitCount m_totalUnits = 0;
    UnitCount m_completedUnits = 0;
    bool m_cancellationRequested = false;
    std::optional<Domain::OperationError> m_error;
    std::optional<OutputReference> m_outputReference;
};

// Owns one report/PDF/export lifecycle. The state owner releases its
// operation-scoped render source and output buffers after recording
// completion, cancellation, or failure; this snapshot retains only the
// bounded output reference from a successful completion. Worker code emits
// events through this owner and never mutates the state directly.
class ReportJobState final
{
public:
    using UnitCount = ReportJobSnapshot::UnitCount;
    using OutputReference = ReportJobSnapshot::OutputReference;

    ReportJobState() = default;

    [[nodiscard]] ReportJobSnapshot snapshot() const
    {
        return m_snapshot;
    }

    [[nodiscard]] Domain::Result<void> start(
        UnitCount totalUnits
        )
    {
        if (m_snapshot.phase() == ReportJobPhase::Running)
        {
            return Domain::Result<void>::failure(
                conflict("Cannot start a report job while another job is running.")
                );
        }

        m_snapshot = ReportJobSnapshot{};
        m_snapshot.m_phase = ReportJobPhase::Running;
        m_snapshot.m_totalUnits = totalUnits;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> reportProgress(
        UnitCount completedUnits
        )
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict("Report progress can only be reported while running.")
                );
        }

        if (completedUnits > m_snapshot.m_totalUnits)
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Report progress cannot exceed the total unit count."
                    )
                );
        }

        m_snapshot.m_completedUnits = completedUnits;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> requestCancellation()
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Report cancellation can only be requested while running."
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
                    "Report cancellation can only be acknowledged while running."
                    )
                );
        }

        m_snapshot.m_phase = ReportJobPhase::Canceled;
        m_snapshot.m_outputReference.reset();
        return Domain::Result<void>::success();
    }

    // Convenience event equivalent to acknowledgeCancellation().
    [[nodiscard]] Domain::Result<void> cancel()
    {
        return acknowledgeCancellation();
    }

    [[nodiscard]] Domain::Result<void> complete(
        OutputReference outputReference
        )
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict("Report can only be completed while running.")
                );
        }

        if (!isValidOutputReference(outputReference))
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Report output reference must be non-blank and bounded."
                    )
                );
        }

        if (m_snapshot.m_completedUnits != m_snapshot.m_totalUnits)
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Report cannot be completed before all units are complete."
                    )
                );
        }

        // A cancellation request is advisory until acknowledged; final
        // progress reported first allows completion to win the race.
        m_snapshot.m_phase = ReportJobPhase::Completed;
        m_snapshot.m_outputReference = std::move(outputReference);
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> fail(
        Domain::OperationError error
        )
    {
        if (!isRunning())
        {
            return Domain::Result<void>::failure(
                conflict("Report can only fail while running.")
                );
        }

        m_snapshot.m_phase = ReportJobPhase::Failed;
        m_snapshot.m_error = std::move(error);
        m_snapshot.m_outputReference.reset();
        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_snapshot.phase() == ReportJobPhase::Running;
    }

    [[nodiscard]] static bool isValidOutputReference(
        const OutputReference& outputReference
        ) noexcept
    {
        if (outputReference.empty()
            || outputReference.size() > kReportJobMaxOutputReferenceLength)
        {
            return false;
        }

        return !std::all_of(
            outputReference.cbegin(),
            outputReference.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
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

    ReportJobSnapshot m_snapshot;
};

}
