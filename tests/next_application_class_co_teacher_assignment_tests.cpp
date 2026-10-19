#include "next/application/class_co_teacher_assignment_use_case.h"

#include <iostream>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

Domain::TeacherId teacherId(const std::string& value)
{
    return *Domain::TeacherId::fromString(value);
}

void requireInvalidInput(
    const Domain::Result<void>& result,
    const std::string& expectedMessage
    )
{
    require(!result, "invalid ID should fail");
    require(result.error().code == Domain::ErrorCode::InvalidInput,
        "invalid ID should use InvalidInput");
    require(result.error().message == expectedMessage,
        "invalid ID should preserve its validation message");
    require(result.error().recoverable,
        "invalid ID should remain recoverable");
}

class FakePort final : public ClassCoTeacherAssignmentPort
{
public:
    [[nodiscard]] Domain::Result<void> assignClassCoTeacher(
        const ClassCoTeacherAssignmentRequest& request
        ) const override
    {
        ++callCount;
        requests.push_back(request);
        if (!succeeds)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Conflict,
                .message = "assignment rejected",
                .recoverable = true
            });
        }
        return Domain::Result<void>::success();
    }

    mutable int callCount = 0;
    mutable std::vector<ClassCoTeacherAssignmentRequest> requests;
    bool succeeds = true;
};

void invalidIdsDoNotReachPersistence()
{
    FakePort port;

    for (const std::string value : {"0", "-1"})
    {
        requireInvalidInput(
            ClassCoTeacherAssignmentUseCase::execute(
                classId(value), teacherId("3"), port
                ),
            "Class ID must be positive."
            );
    }

    for (const std::string value : {"0", "-2", "-01"})
    {
        requireInvalidInput(
            ClassCoTeacherAssignmentUseCase::execute(
                classId("8"), teacherId(value), port
                ),
            "Teacher ID must be positive or unassigned."
            );
    }

    for (const std::string value : {"class", "8tail", "2147483648"})
    {
        requireInvalidInput(
            ClassCoTeacherAssignmentUseCase::execute(
                classId(value), teacherId("3"), port
                ),
            "Class or teacher ID is invalid."
            );
    }

    for (const std::string value : {
             "teacher", "21tail", "9223372036854775808"
         })
    {
        requireInvalidInput(
            ClassCoTeacherAssignmentUseCase::execute(
                classId("8"), teacherId(value), port
                ),
            "Class or teacher ID is invalid."
            );
    }

    require(port.callCount == 0,
        "invalid IDs must not reach persistence");
}

void positiveTeacherIdUsesTypedRequest()
{
    FakePort port;
    const Domain::ClassId requestedClassId = classId("0008");
    const Domain::TeacherId requestedTeacherId = teacherId("00021");
    const auto result = ClassCoTeacherAssignmentUseCase::execute(
        requestedClassId,
        requestedTeacherId,
        port
        );

    require(result.hasValue(), "valid assignment should succeed");
    require(port.callCount == 1 && port.requests.size() == 1,
        "valid assignment should reach persistence once");
    require(port.requests.front().classId == requestedClassId,
        "request should preserve the supplied typed class identity");
    require(port.requests.front().teacherId.has_value()
                && *port.requests.front().teacherId == requestedTeacherId,
        "request should preserve the supplied typed teacher identity");
}

void widePositiveTeacherIdUsesTypedRequest()
{
    FakePort port;
    constexpr std::int64_t wideTeacherId = 2147483648LL;
    const auto result = ClassCoTeacherAssignmentUseCase::execute(
        classId("8"),
        teacherId(std::to_string(wideTeacherId)),
        port
        );

    require(result.hasValue(), "wide valid assignment should succeed");
    require(port.callCount == 1 && port.requests.size() == 1,
        "wide valid assignment should reach persistence once");
    require(port.requests.front().teacherId.has_value()
                && port.requests.front().teacherId->value() == "2147483648",
        "request should preserve the full teacher identity");
}

void maximumIdsReachTypedPort()
{
    FakePort classIdPort;
    const auto maximumClassIdResult =
        ClassCoTeacherAssignmentUseCase::execute(
            classId("2147483647"),
            teacherId("21"),
            classIdPort
            );

    require(maximumClassIdResult.hasValue(),
        "maximum int class ID should be valid");
    require(classIdPort.callCount == 1
                && classIdPort.requests.front().classId.value() == "2147483647",
        "maximum int class ID should reach the typed port unchanged");

    FakePort teacherIdPort;
    const auto maximumTeacherIdResult =
        ClassCoTeacherAssignmentUseCase::execute(
            classId("8"),
            teacherId("9223372036854775807"),
            teacherIdPort
            );

    require(maximumTeacherIdResult.hasValue(),
        "maximum int64 teacher ID should be valid");
    require(teacherIdPort.callCount == 1
                && teacherIdPort.requests.front().teacherId.has_value()
                && teacherIdPort.requests.front().teacherId->value()
                    == "9223372036854775807",
        "maximum int64 teacher ID should reach the typed port unchanged");
}

void unassignedSentinelBecomesMissingTeacherId()
{
    FakePort port;
    const auto result = ClassCoTeacherAssignmentUseCase::execute(
        classId("8"),
        teacherId("-1"),
        port
        );

    require(result.hasValue(), "unassigned co-teacher should be valid");
    require(port.callCount == 1 && port.requests.size() == 1,
        "unassigned co-teacher should reach persistence once");
    require(!port.requests.front().teacherId,
        "legacy -1 sentinel should become an absent typed teacher ID");
}

void persistenceFailureIsPreserved()
{
    FakePort port;
    port.succeeds = false;

    const auto result = ClassCoTeacherAssignmentUseCase::execute(
        classId("8"),
        teacherId("21"),
        port
        );

    require(!result && result.error().code == Domain::ErrorCode::Conflict
                && result.error().message == "assignment rejected"
                && result.error().recoverable,
        "persistence failure should be returned unchanged");
    require(port.callCount == 1,
        "valid request should reach persistence even when it fails");
}

} // namespace

int main()
{
    try
    {
        invalidIdsDoNotReachPersistence();
        positiveTeacherIdUsesTypedRequest();
        widePositiveTeacherIdUsesTypedRequest();
        maximumIdsReachTypedPort();
        unassignedSentinelBecomesMissingTeacherId();
        persistenceFailureIsPreserved();
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
