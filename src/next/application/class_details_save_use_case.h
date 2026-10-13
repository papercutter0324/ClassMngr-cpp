#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/domain/schedule_time.h"

#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassDetailsSaveRequest final
{
    Domain::ClassId classId;
    std::u16string classGrade;
    std::u16string classLevel;
    std::u16string readingBook;
    std::u16string essayBook;
    std::u16string classColor;
    std::u16string fontColor;
    std::optional<std::vector<Domain::ScheduleTime>> regularTimes;
    std::optional<std::vector<Domain::ScheduleTime>> intensiveTimes;
    std::optional<Domain::TeacherId> teacherId;
};

class ClassDetailsSavePort
{
public:
    virtual ~ClassDetailsSavePort() = default;

    [[nodiscard]] virtual Domain::Result<void> saveClassDetails(
        const ClassDetailsSaveRequest& request
        ) const = 0;
};

class ClassDetailsSaveUseCase final
{
public:
    [[nodiscard]] static Domain::Result<void> execute(
        const ClassDetailsSaveRequest& request,
        const ClassDetailsSavePort& port
        )
    {
        if (!parsePositiveDecimalId(request.classId.value()))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a positive integer.",
                .recoverable = true
            });
        }

        if (request.teacherId
            && !isCanonicalPositiveDecimalId64(request.teacherId->value()))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Teacher ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        return port.saveClassDetails(request);
    }
};

} // namespace ClassMngr::Next::Application
