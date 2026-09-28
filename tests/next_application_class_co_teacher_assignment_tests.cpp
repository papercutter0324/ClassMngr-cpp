#include "next/application/class_co_teacher_assignment_use_case.h"

#include <iostream>
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

    for (const int classId : {0, -1})
    {
        const auto result =
            ClassCoTeacherAssignmentUseCase::execute(classId, 3, port);
        require(!result && result.error().code == Domain::ErrorCode::InvalidInput,
            "nonpositive class ID should be rejected");
    }

    for (const int teacherId : {0, -2})
    {
        const auto result =
            ClassCoTeacherAssignmentUseCase::execute(8, teacherId, port);
        require(!result && result.error().code == Domain::ErrorCode::InvalidInput,
            "invalid teacher ID should be rejected");
    }

    require(port.callCount == 0,
        "invalid IDs must not reach persistence");
}

void positiveTeacherIdUsesTypedRequest()
{
    FakePort port;
    const auto result = ClassCoTeacherAssignmentUseCase::execute(8, 21, port);

    require(result.hasValue(), "valid assignment should succeed");
    require(port.callCount == 1 && port.requests.size() == 1,
        "valid assignment should reach persistence once");
    require(port.requests.front().classId.value() == "8",
        "request should carry a typed class ID");
    require(port.requests.front().teacherId.has_value()
                && port.requests.front().teacherId->value() == "21",
        "request should carry a typed teacher ID");
}

void unassignedSentinelBecomesMissingTeacherId()
{
    FakePort port;
    const auto result = ClassCoTeacherAssignmentUseCase::execute(8, -1, port);

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

    const auto result = ClassCoTeacherAssignmentUseCase::execute(8, 21, port);

    require(!result && result.error().code == Domain::ErrorCode::Conflict
                && result.error().message == "assignment rejected",
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
