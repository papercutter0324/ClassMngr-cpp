#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

struct ClassDetailsValidationContextData final
{
    Domain::ClassId matchedClassId;
    int teacherId = -1;
    std::u16string notes;
    std::u16string timeFillerActivities;
};

struct ClassDetailsValidationContextSnapshot final
{
    Domain::ClassId requestedClassId;
    Domain::ClassId matchedClassId;
    int teacherId = -1;
    std::u16string notes;
    std::u16string timeFillerActivities;
};

using ClassDetailsValidationContextPortResult =
    Domain::Result<ClassDetailsValidationContextData>;
using ClassDetailsValidationContextResult =
    Domain::Result<ClassDetailsValidationContextSnapshot>;

class ClassDetailsValidationContextPort
{
public:
    virtual ~ClassDetailsValidationContextPort() = default;

    [[nodiscard]] virtual ClassDetailsValidationContextPortResult
    loadClassDetailsValidationContext(
        const Domain::ClassId& classId
        ) const = 0;
};

class ClassDetailsValidationContextQuery final
{
public:
    [[nodiscard]] static ClassDetailsValidationContextResult execute(
        const Domain::ClassId& classId,
        const ClassDetailsValidationContextPort& port
        )
    {
        if (!isCanonicalPositiveInteger(classId.value()))
        {
            return ClassDetailsValidationContextResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        const ClassDetailsValidationContextPortResult loaded =
            port.loadClassDetailsValidationContext(classId);
        if (!loaded)
        {
            return ClassDetailsValidationContextResult::failure(
                loaded.error()
                );
        }

        const ClassDetailsValidationContextData& context = loaded.value();
        if (context.matchedClassId != classId)
        {
            return ClassDetailsValidationContextResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A class validation context returned a different class identifier.",
                .recoverable = false
            });
        }

        return ClassDetailsValidationContextResult::success({
            .requestedClassId = classId,
            .matchedClassId = context.matchedClassId,
            .teacherId = context.teacherId,
            .notes = context.notes,
            .timeFillerActivities = context.timeFillerActivities
        });
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveInteger(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
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
        return true;
    }
};

} // namespace ClassMngr::Next::Application
