#pragma once

#include "next/application/class_summary_projection.h"
#include "next/application/sub_prep_schedule_summary_query.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <utility>

namespace ClassMngr::Next::Application
{

// An immutable snapshot of the selected Sub Prep class, its teacher identity,
// and its one bounded detail value. Transitions return a replacement snapshot
// or a structured error, leaving this value unchanged.
class SubPrepClassInformationState final
{
public:
    SubPrepClassInformationState() = default;

    [[nodiscard]] const std::optional<Domain::ClassId>& selectedClassId() const
        noexcept
    {
        return m_selectedClassId;
    }

    [[nodiscard]] const std::optional<Domain::TeacherId>& selectedTeacherId()
        const noexcept
    {
        return m_selectedTeacherId;
    }

    [[nodiscard]] const std::optional<SubPrepClassDetails>& details() const
        noexcept
    {
        return m_details;
    }

    [[nodiscard]] Domain::Result<SubPrepClassInformationState> refreshScope(
        const SubPrepScheduleSummaryQueryResult& summaryResult
        ) const
    {
        if (!summaryResult)
        {
            return Domain::Result<SubPrepClassInformationState>::failure(
                summaryResult.error()
                );
        }

        auto selectedClassId = m_selectedClassId;
        std::optional<Domain::TeacherId> selectedTeacherId;
        if (selectedClassId.has_value())
        {
            const auto selectedClass =
                summaryResult.value().findClass(*selectedClassId);
            if (selectedClass.has_value())
            {
                selectedTeacherId = selectedClass->teacherId;
            }
            else
            {
                selectedClassId.reset();
            }
        }

        return Domain::Result<SubPrepClassInformationState>::success(
            SubPrepClassInformationState(
                std::move(selectedClassId),
                std::move(selectedTeacherId),
                std::nullopt
                )
            );
    }

    [[nodiscard]] Domain::Result<SubPrepClassInformationState>
    selectVisibleClass(
        const Domain::ClassId& classId,
        const ClassSummaryProjection& visibleClasses
        ) const
    {
        const auto selectedClass = visibleClasses.findClass(classId);
        if (!selectedClass.has_value())
        {
            return Domain::Result<SubPrepClassInformationState>::failure(
                notFound("The selected class is not visible in the current scope.")
                );
        }

        const auto selectedTeacherId = selectedClass->teacherId;
        if (m_selectedClassId == classId)
        {
            if (m_selectedTeacherId != selectedTeacherId)
            {
                return Domain::Result<SubPrepClassInformationState>::success(
                    SubPrepClassInformationState(
                        m_selectedClassId,
                        selectedTeacherId,
                        std::nullopt
                        )
                    );
            }

            return Domain::Result<SubPrepClassInformationState>::success(*this);
        }

        return Domain::Result<SubPrepClassInformationState>::success(
            SubPrepClassInformationState(
                classId,
                selectedTeacherId,
                std::nullopt
                )
            );
    }

    [[nodiscard]] Domain::Result<SubPrepClassInformationState> applyDetails(
        SubPrepClassDetails details
        ) const
    {
        if (!m_selectedClassId.has_value())
        {
            return Domain::Result<SubPrepClassInformationState>::failure(
                invalidInput("Class details require a current class selection.")
                );
        }

        if (details.classId != *m_selectedClassId)
        {
            return Domain::Result<SubPrepClassInformationState>::failure(
                validationError("Class details do not match the current selection.")
                );
        }

        if (details.teacherId != m_selectedTeacherId)
        {
            return Domain::Result<SubPrepClassInformationState>::failure(
                validationError(
                    "Class details do not match the selected class summary."
                    )
                );
        }

        const auto validation =
            ClassSummaryProjectionDetail::validateSelectedDetailsValue(details);
        if (!validation)
        {
            return Domain::Result<SubPrepClassInformationState>::failure(
                validation.error()
                );
        }

        return Domain::Result<SubPrepClassInformationState>::success(
            SubPrepClassInformationState(
                m_selectedClassId,
                m_selectedTeacherId,
                std::move(details)
                )
            );
    }

    [[nodiscard]] SubPrepClassInformationState clear() const
    {
        return {};
    }

    friend bool operator==(
        const SubPrepClassInformationState&,
        const SubPrepClassInformationState&
        ) = default;

private:
    SubPrepClassInformationState(
        std::optional<Domain::ClassId> selectedClassId,
        std::optional<Domain::TeacherId> selectedTeacherId,
        std::optional<SubPrepClassDetails> details
        )
        : m_selectedClassId(std::move(selectedClassId)),
          m_selectedTeacherId(std::move(selectedTeacherId)),
          m_details(std::move(details))
    {
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

    [[nodiscard]] static Domain::OperationError notFound(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::NotFound,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError validationError(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Validation,
            .message = message,
            .recoverable = false
        };
    }

    std::optional<Domain::ClassId> m_selectedClassId;
    std::optional<Domain::TeacherId> m_selectedTeacherId;
    std::optional<SubPrepClassDetails> m_details;
};

} // namespace ClassMngr::Next::Application
