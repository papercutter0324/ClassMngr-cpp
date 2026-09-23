#pragma once

#include "next/application/class_summary_projection.h"

#include <utility>

namespace ClassMngr::Next::Application
{

using SubPrepClassDetailsReadResult = Domain::Result<SubPrepClassDetails>;

// Reads one copied selected-class detail value. Implementations must not
// retain the typed request reference after this call returns.
class SubPrepClassDetailsReadPort
{
public:
    virtual ~SubPrepClassDetailsReadPort() = default;

    [[nodiscard]] virtual SubPrepClassDetailsReadResult loadDetails(
        const Domain::ClassId& classId
        ) = 0;
};

using SubPrepClassDetailsQueryResult = Domain::Result<SubPrepClassDetails>;

// This query owns no cache or UI state. It validates the request before I/O,
// then returns one complete bounded detail value or a structured error.
class SubPrepClassDetailsQuery final
{
public:
    explicit SubPrepClassDetailsQuery(
        SubPrepClassDetailsReadPort& readPort
        ) noexcept
        : m_readPort(readPort)
    {
    }

    [[nodiscard]] SubPrepClassDetailsQueryResult execute(
        const Domain::ClassId& classId
        ) const
    {
        if (!ClassSummaryProjectionDetail::isValidId(classId))
        {
            return SubPrepClassDetailsQueryResult::failure(
                invalidInput(
                    "Selected class identifier must be non-blank and bounded."
                    )
                );
        }

        auto source = m_readPort.loadDetails(classId);
        if (!source)
        {
            return SubPrepClassDetailsQueryResult::failure(source.error());
        }

        auto details = std::move(source.value());
        if (details.classId != classId)
        {
            return SubPrepClassDetailsQueryResult::failure(
                validationError(
                    "A selected-class details read returned a different class identifier."
                    )
                );
        }

        const auto detailsValidation =
            ClassSummaryProjectionDetail::validateSelectedDetailsValue(
                details
                );
        if (!detailsValidation)
        {
            return SubPrepClassDetailsQueryResult::failure(
                detailsValidation.error()
                );
        }

        return SubPrepClassDetailsQueryResult::success(std::move(details));
    }

private:
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

    SubPrepClassDetailsReadPort& m_readPort;
};

} // namespace ClassMngr::Next::Application
