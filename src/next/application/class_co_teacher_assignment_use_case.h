#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
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
    // typed sentinel here so it does not cross the application port.
    [[nodiscard]] static Domain::Result<void> execute(
        const Domain::ClassId& classId,
        const Domain::TeacherId& teacherId,
        const ClassCoTeacherAssignmentPort& port
        )
    {
        const std::string& classValue = classId.value();
        int parsedClassId = 0;
        const auto [classEnd, classError] = std::from_chars(
            classValue.data(),
            classValue.data() + classValue.size(),
            parsedClassId,
            10
            );
        if (classError == std::errc{}
            && classEnd == classValue.data() + classValue.size()
            && parsedClassId <= 0)
        {
            return invalidInput("Class ID must be positive.");
        }
        if (classError != std::errc{}
            || classEnd != classValue.data() + classValue.size())
        {
            return invalidInput("Class or teacher ID is invalid.");
        }

        const std::string& teacherValue = teacherId.value();
        std::optional<Domain::TeacherId> requestTeacherId;
        if (teacherValue != "-1")
        {
            std::int64_t parsedTeacherId = 0;
            const auto [teacherEnd, teacherError] = std::from_chars(
                teacherValue.data(),
                teacherValue.data() + teacherValue.size(),
                parsedTeacherId,
                10
                );
            if (teacherError == std::errc{}
                && teacherEnd == teacherValue.data() + teacherValue.size()
                && parsedTeacherId <= 0)
            {
                return invalidInput(
                    "Teacher ID must be positive or unassigned."
                    );
            }
            if (teacherError != std::errc{}
                || teacherEnd != teacherValue.data() + teacherValue.size())
            {
                return invalidInput("Class or teacher ID is invalid.");
            }
            requestTeacherId = teacherId;
        }

        return port.assignClassCoTeacher({
            .classId = classId,
            .teacherId = requestTeacherId
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
