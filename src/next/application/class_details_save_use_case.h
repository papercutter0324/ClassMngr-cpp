#pragma once

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
        if (!isPositiveInteger(request.classId.value()))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a positive integer.",
                .recoverable = true
            });
        }

        return port.saveClassDetails(request);
    }

private:
    [[nodiscard]] static bool isPositiveInteger(const std::string& value)
    {
        if (value.empty())
        {
            return false;
        }

        for (const char character : value)
        {
            if (character < '0' || character > '9')
            {
                return false;
            }
        }

        return value.find_first_not_of('0') != std::string::npos;
    }
};

} // namespace ClassMngr::Next::Application
