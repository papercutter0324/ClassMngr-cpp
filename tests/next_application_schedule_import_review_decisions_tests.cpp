#include "next/application/schedule_import_review_decisions.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{
using TeacherAction = ScheduleImportReviewTeacherAction;
using ClassAction = ScheduleImportReviewClassAction;
using IssueCode = ScheduleImportReviewDecisionIssueCode;
using Request = ScheduleImportReviewDecisionRequest;

Request validRequest()
{
    Request request;
    request.candidates = {
        {"teacher-a", {"415", "416"}},
        {"teacher-a", {"416"}}
    };
    request.teachers = {
        {"teacher-a", TeacherAction::Reuse, "415"}
    };
    request.classes = {
        {0, ClassAction::UpdateExisting, 21},
        {1, ClassAction::Skip, 22}
    };
    return request;
}

bool hasIssue(
    const ScheduleImportReviewDecisionResult& result,
    IssueCode expected
    )
{
    return std::any_of(
        result.issues.begin(),
        result.issues.end(),
        [expected](const auto& issue)
        {
            return issue.code == expected;
        }
        );
}

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void expectRejected(
    const std::string& name,
    const Request& request,
    IssueCode expected
    )
{
    const auto result = validateScheduleImportReviewDecisions(request);
    require(!result.accepted(), name + " unexpectedly accepted");
    require(hasIssue(result, expected), name + " returned the wrong issue");
}
}

int main()
{
    int cases = 0;
    try
    {
        {
            const auto result =
                validateScheduleImportReviewDecisions(validRequest());
            require(result.accepted(), "complete valid decisions rejected");
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers.clear();
            expectRejected(
                "missing teacher decision",
                request,
                IssueCode::MissingTeacherResolution
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.classes.pop_back();
            expectRejected(
                "missing class decision",
                request,
                IssueCode::MissingClassResolution
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers.push_back(
                {"teacher-a", TeacherAction::Reuse, "416"}
                );
            expectRejected(
                "duplicate teacher decision",
                request,
                IssueCode::DuplicateTeacherResolution
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers.push_back({"", TeacherAction::Skip, {}});
            expectRejected(
                "empty teacher key",
                request,
                IssueCode::EmptyTeacherKey
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers.push_back(
                {"unknown", TeacherAction::Skip, {}}
                );
            expectRejected(
                "foreign teacher decision",
                request,
                IssueCode::UnknownTeacherResolution
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].action = static_cast<TeacherAction>(99);
            expectRejected(
                "invalid teacher action",
                request,
                IssueCode::InvalidTeacherAction
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.classes.push_back(
                {0, ClassAction::CreateNew, -1}
                );
            expectRejected(
                "duplicate class decision",
                request,
                IssueCode::DuplicateClassResolution
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.classes.push_back(
                {2, ClassAction::Skip, -1}
                );
            expectRejected(
                "out of range class decision",
                request,
                IssueCode::CandidateIndexOutOfRange
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.classes[0].action = static_cast<ClassAction>(99);
            expectRejected(
                "invalid class action",
                request,
                IssueCode::InvalidClassAction
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.classes[0].targetClassId = 0;
            expectRejected(
                "update without target",
                request,
                IssueCode::UpdateClassMissingTarget
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.classes[0] = {0, ClassAction::CreateNew, 21};
            expectRejected(
                "create with existing target",
                request,
                IssueCode::CreateNewClassHasTarget
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.classes[1] = {1, ClassAction::UpdateExisting, 21};
            expectRejected(
                "duplicate updated class target",
                request,
                IssueCode::DuplicateUpdatedClassTarget
                );
            const auto result =
                validateScheduleImportReviewDecisions(request);
            const auto issue = std::find_if(
                result.issues.begin(),
                result.issues.end(),
                [](const auto& value)
                {
                    return value.code
                        == IssueCode::DuplicateUpdatedClassTarget;
                }
                );
            require(
                issue != result.issues.end()
                    && issue->targetClassId == 21
                    && issue->candidateIndexes == std::vector<int>({0, 1}),
                "duplicate target issue omitted its UI detail"
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.classes[0] = {0, ClassAction::Skip, 22};
            request.classes[1] = {1, ClassAction::Skip, 22};
            expectRejected(
                "duplicate skipped class target",
                request,
                IssueCode::DuplicateSkippedClassTarget
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].action = TeacherAction::Create;
            request.teachers[0].selectedRoom.clear();
            expectRejected(
                "create without imported room",
                request,
                IssueCode::MissingTeacherRoom
            );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].action = TeacherAction::Create;
            request.teachers[0].selectedRoom = "999";
            expectRejected(
                "create with foreign room",
                request,
                IssueCode::ForeignTeacherRoom
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].action = TeacherAction::UpdateRoom;
            request.teachers[0].selectedRoom.clear();
            expectRejected(
                "room update without imported room",
                request,
                IssueCode::MissingTeacherRoom
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].action = TeacherAction::UpdateRoom;
            request.teachers[0].selectedRoom = "999";
            expectRejected(
                "room update with foreign room",
                request,
                IssueCode::ForeignTeacherRoom
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].selectedRoom.clear();
            expectRejected(
                "reuse with multiple rooms and no selection",
                request,
                IssueCode::MissingTeacherRoom
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].selectedRoom = "999";
            expectRejected(
                "reuse with multiple rooms and foreign selection",
                request,
                IssueCode::ForeignTeacherRoom
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].selectedRoom = "416";
            require(
                validateScheduleImportReviewDecisions(request).accepted(),
                "reuse with selected imported room rejected"
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.candidates[0].importedRooms.clear();
            request.candidates[1].importedRooms.clear();
            request.teachers[0].selectedRoom.clear();
            require(
                validateScheduleImportReviewDecisions(request).accepted(),
                "reuse with zero imported rooms changed meaning"
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.candidates[0].importedRooms = {"415"};
            request.candidates[1].importedRooms = {"415"};
            request.teachers[0].selectedRoom.clear();
            require(
                validateScheduleImportReviewDecisions(request).accepted(),
                "reuse with one imported room changed meaning"
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.candidates[0].importedRooms = {"415"};
            request.candidates[1].importedRooms = {"415"};
            request.teachers[0].selectedRoom = "999";
            require(
                validateScheduleImportReviewDecisions(request).accepted(),
                "reuse with one imported room began validating an optional choice"
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].action = TeacherAction::Skip;
            request.teachers[0].selectedRoom = "foreign";
            request.classes[0] = {0, ClassAction::CreateNew, -1};
            expectRejected(
                "active class assigned to skipped teacher",
                request,
                IssueCode::ActiveClassAssignedToSkippedTeacher
                );
            ++cases;
        }
        {
            auto request = validRequest();
            request.teachers[0].action = TeacherAction::Skip;
            request.classes[0] = {0, ClassAction::Skip, 21};
            request.classes[1] = {1, ClassAction::Skip, 22};
            require(
                validateScheduleImportReviewDecisions(request).accepted(),
                "skipped classes lost their existing target meaning"
                );
            ++cases;
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::cout << cases << " Schedule Import decision cases passed\n";
    return 0;
}
