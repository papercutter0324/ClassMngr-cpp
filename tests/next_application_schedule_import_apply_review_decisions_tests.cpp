#include "next/application/schedule_import_apply_review_decisions.h"
#include "next/application/schedule_import_apply_use_case.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next::Application;

static_assert(std::is_same_v<
    decltype(ScheduleImportApplyCandidate{}.teacherKey),
    ScheduleImportApplyTeacherKey
    >);
static_assert(std::is_same_v<
    decltype(ScheduleImportApplyTeacher{}.teacherKey),
    ScheduleImportApplyTeacherKey
    >);
static_assert(std::is_same_v<
    decltype(ScheduleImportApplyTeacherTargetIssue{}.teacherKey),
    ScheduleImportApplyTeacherKey
    >);
static_assert(!std::is_convertible_v<
    std::u16string,
    ScheduleImportApplyTeacherKey
    >);

namespace
{
void require(const bool condition)
{
    if (!condition)
    {
        throw std::runtime_error(
            "Schedule import apply review decision projection assertion failed"
            );
    }
}
}

int main()
{
    constexpr char KoreanTeacherAndTestTube[] =
        "\xEA\xB9\x80\xF0\x9F\xA7\xAA";
    constexpr char Rocket[] = "\xF0\x9F\x9A\x80";

    const ScheduleImportApplyTeacherKey defaultKey;
    require(defaultKey.empty());
    require(defaultKey.value().empty());
    const std::u16string exactKey = u"\uAE40\U0001F9EA";
    const ScheduleImportApplyTeacherKey wrappedKey{exactKey};
    require(!wrappedKey.empty());
    require(wrappedKey.value() == exactKey);
    require(wrappedKey == ScheduleImportApplyTeacherKey{exactKey});
    const ScheduleImportApplyTeacherKey composedKey{u"\u00E9"};
    const ScheduleImportApplyTeacherKey decomposedKey{u"e\u0301"};
    require(composedKey.value() == u"\u00E9");
    require(decomposedKey.value() == u"e\u0301");
    require(composedKey != decomposedKey);
    require(
        ScheduleImportApplyTeacherKey{u"A"}
        < ScheduleImportApplyTeacherKey{u"B"}
        );

    ScheduleImportApplyRequest request;
    ScheduleImportApplyCandidate firstCandidate;
    firstCandidate.teacherKey = ScheduleImportApplyTeacherKey{
        u"\uAE40\U0001F9EA"
    };
    firstCandidate.rooms = {u"", u"414", u"\U0001F680"};
    request.candidates.push_back(firstCandidate);

    ScheduleImportApplyCandidate secondCandidate;
    secondCandidate.teacherKey = ScheduleImportApplyTeacherKey{
        u"\uC774\uC120\uC0DD"
    };
    secondCandidate.rooms = {u"", u"502"};
    request.candidates.push_back(secondCandidate);

    ScheduleImportApplyCandidate thirdCandidate;
    thirdCandidate.teacherKey = ScheduleImportApplyTeacherKey{
        u"\uBC15\uC0DD"
    };
    thirdCandidate.rooms = {u""};
    request.candidates.push_back(thirdCandidate);

    const auto teacherId = ClassMngr::Next::Domain::TeacherId::fromString(
        "teacher-7"
        );
    const auto skippedClassId = ClassMngr::Next::Domain::ClassId::fromString(
        "class-42"
        );
    const auto updatedClassId = ClassMngr::Next::Domain::ClassId::fromString(
        "class-17"
        );
    require(teacherId.has_value());
    require(skippedClassId.has_value());
    require(updatedClassId.has_value());

    request.teachers = {
        {
            ScheduleImportApplyTeacherKey{u"\uC774\uC120\uC0DD"},
            ScheduleImportReviewTeacherAction::Skip,
            std::nullopt,
            u""
        },
        {
            ScheduleImportApplyTeacherKey{u"\uAE40\U0001F9EA"},
            ScheduleImportReviewTeacherAction::UpdateRoom,
            teacherId,
            u"\U0001F680"
        },
        {
            ScheduleImportApplyTeacherKey{u"\uBC15\uC0DD"},
            ScheduleImportReviewTeacherAction::Create,
            std::nullopt,
            u""
        }
    };
    request.classes = {
        {
            1,
            ScheduleImportReviewClassAction::Skip,
            skippedClassId,
            u"",
            u""
        },
        {
            0,
            ScheduleImportReviewClassAction::UpdateExisting,
            updatedClassId,
            u"",
            u""
        },
        {
            2,
            ScheduleImportReviewClassAction::CreateNew,
            std::nullopt,
            u"",
            u""
        }
    };

    const ScheduleImportReviewDecisionRequest decisions =
        projectScheduleImportApplyReviewDecisions(request);

    require(decisions.candidates.size() == 3);
    require(
        decisions.candidates[0].teacherKey.value() == KoreanTeacherAndTestTube
        );
    require(decisions.candidates[0].importedRooms.size() == 2);
    require(decisions.candidates[0].importedRooms[0] == "414");
    require(decisions.candidates[0].importedRooms[1] == Rocket);
    require(
        decisions.candidates[1].teacherKey.value()
        == "\xEC\x9D\xB4\xEC\x84\xA0\xEC\x83\x9D"
        );
    require(decisions.candidates[1].importedRooms.size() == 1);
    require(decisions.candidates[1].importedRooms[0] == "502");
    require(
        decisions.candidates[2].teacherKey.value() == "\xEB\xB0\x95\xEC\x83\x9D"
        );
    require(decisions.candidates[2].importedRooms.empty());

    require(decisions.teachers.size() == 3);
    require(
        decisions.teachers[0].teacherKey.value()
        == "\xEC\x9D\xB4\xEC\x84\xA0\xEC\x83\x9D"
        );
    require(
        decisions.teachers[0].action
        == ScheduleImportReviewTeacherAction::Skip
        );
    require(decisions.teachers[1].teacherKey.value() == KoreanTeacherAndTestTube);
    require(
        decisions.teachers[1].action
        == ScheduleImportReviewTeacherAction::UpdateRoom
        );
    require(decisions.teachers[1].selectedRoom == Rocket);
    require(decisions.teachers[2].teacherKey.value() == "\xEB\xB0\x95\xEC\x83\x9D");
    require(decisions.teachers[2].action == ScheduleImportReviewTeacherAction::Create);

    require(decisions.classes.size() == 3);
    require(decisions.classes[0].candidateIndex == 1);
    require(
        decisions.classes[0].action == ScheduleImportReviewClassAction::Skip
        );
    require(decisions.classes[0].targetClassId == skippedClassId);
    require(decisions.classes[1].candidateIndex == 0);
    require(
        decisions.classes[1].action
        == ScheduleImportReviewClassAction::UpdateExisting
        );
    require(decisions.classes[1].targetClassId == updatedClassId);
    require(decisions.classes[2].candidateIndex == 2);
    require(
        decisions.classes[2].action == ScheduleImportReviewClassAction::CreateNew
        );
    require(!decisions.classes[2].targetClassId);
}
