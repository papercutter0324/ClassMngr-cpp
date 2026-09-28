#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

struct ClassCoTeacherAssignmentRequest final
{
    Domain::ClassId classId;
    std::optional<Domain::TeacherId> teacherId;
};

class ClassCoTeacherAssignmentPort
{
public:
    virtual ~ClassCoTeacherAssignmentPort() = default;

    [[nodiscard]] virtual Domain::Result<void> assignClassCoTeacher(
        const ClassCoTeacherAssignmentRequest& request
        ) const = 0;
};

class ClassCoTeacherAssignmentUseCase final
{
public:
    // The legacy page uses -1 for an unassigned co-teacher. Translate that
    // sentinel here so it does not cross the application port.
    [[nodiscard]] static Domain::Result<void> execute(
        const int classId,
        const int teacherId,
        const ClassCoTeacherAssignmentPort& port
        )
    {
        if (classId <= 0)
        {
            return invalidInput("Class ID must be positive.");
        }

        if (teacherId <= 0 && teacherId != -1)
        {
            return invalidInput(
                "Teacher ID must be positive or unassigned."
                );
        }

        const auto typedClassId = Domain::ClassId::fromString(
            std::to_string(classId)
            );
        const std::optional<Domain::TeacherId> typedTeacherId =
            teacherId == -1
                ? std::nullopt
                : Domain::TeacherId::fromString(std::to_string(teacherId));
        if (!typedClassId || (teacherId > 0 && !typedTeacherId))
        {
            return invalidInput("Class or teacher ID is invalid.");
        }

        return port.assignClassCoTeacher({
            .classId = *typedClassId,
            .teacherId = typedTeacherId
        });
    }

private:
    [[nodiscard]] static Domain::Result<void> invalidInput(
        std::string message
        )
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = std::move(message),
            .recoverable = true
        });
    }
};

} // namespace ClassMngr::Next::Application
