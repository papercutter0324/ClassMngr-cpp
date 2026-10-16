#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/domain/schedule_time.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class ClassDetailsScheduleMode
{
    Regular,
    Intensive
};

struct ClassDetailsScheduleConflictRequest final
{
    Domain::ClassId classId;
    ClassDetailsScheduleMode mode = ClassDetailsScheduleMode::Regular;
    std::vector<Domain::ScheduleTime> candidateTimes;
};

struct ClassDetailsScheduleConflict final
{
    std::u16string className;
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;
    std::u16string conflictingClassName;

    friend bool operator==(
        const ClassDetailsScheduleConflict&,
        const ClassDetailsScheduleConflict&
        ) = default;
};

using ClassDetailsScheduleConflictResult =
    Domain::Result<std::vector<ClassDetailsScheduleConflict>>;

class ClassDetailsScheduleConflictPort
{
public:
    virtual ~ClassDetailsScheduleConflictPort() = default;

    [[nodiscard]] virtual ClassDetailsScheduleConflictResult
    classDetailsScheduleConflicts(
        const ClassDetailsScheduleConflictRequest& request
        ) const = 0;
};

class ClassDetailsScheduleConflictQuery final
{
public:
    [[nodiscard]] static ClassDetailsScheduleConflictResult execute(
        const ClassDetailsScheduleConflictRequest& request,
        const ClassDetailsScheduleConflictPort& port
        )
    {
        if (!isCanonicalPositiveDecimalId(request.classId.value()))
        {
            return ClassDetailsScheduleConflictResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        if (request.mode != ClassDetailsScheduleMode::Regular
            && request.mode != ClassDetailsScheduleMode::Intensive)
        {
            return ClassDetailsScheduleConflictResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "The class-details schedule mode is invalid.",
                .recoverable = true
            });
        }

        return port.classDetailsScheduleConflicts(request);
    }
};

} // namespace ClassMngr::Next::Application
