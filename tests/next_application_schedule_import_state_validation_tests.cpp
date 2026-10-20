#include "next/application/schedule_import_state_validation.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;
using ClassMngr::Next::Domain::Weekday;
namespace Domain = ClassMngr::Next::Domain;
using ReviewTeacherKey = ScheduleImportReviewTeacherKey;

static_assert(!std::is_convertible_v<Domain::TeacherId, Domain::ClassId>);
static_assert(!std::is_convertible_v<Domain::ClassId, Domain::TeacherId>);
static_assert(!std::is_assignable_v<Domain::TeacherId&, Domain::ClassId>);
static_assert(!std::is_assignable_v<Domain::ClassId&, Domain::TeacherId>);
static_assert(std::is_same_v<
              decltype(ScheduleImportStateCandidate{}.teacherKey),
              ReviewTeacherKey>);
static_assert(std::is_same_v<
              decltype(ScheduleImportStateTeacherResolution{}.teacherKey),
              ReviewTeacherKey>);
static_assert(std::is_same_v<
              decltype(std::declval<ScheduleImportStateTeacherSnapshot>().teacherKey),
              ReviewTeacherKey>);
static_assert(!std::is_convertible_v<std::string, ReviewTeacherKey>);
static_assert(std::is_same_v<
              decltype(ScheduleImportStateTeacherResolution{}.targetTeacherId),
              std::optional<Domain::TeacherId>>);
static_assert(std::is_same_v<
              decltype(ScheduleImportStateClassResolution{}.targetClassId),
              std::optional<Domain::ClassId>>);
static_assert(std::is_same_v<
              decltype(ScheduleImportStateTime{}.scheduleTime),
              std::optional<Domain::ScheduleTime>>);
using MakeScheduleImportStateTime = ScheduleImportStateTime (*) (
    std::optional<Domain::Weekday>,
    int,
    int,
    std::string,
    std::string,
    std::string
    );
static_assert(std::is_same_v<
    decltype(&makeScheduleImportStateTime),
    MakeScheduleImportStateTime
    >);
static_assert(std::is_same_v<
              decltype(std::declval<ScheduleImportStateTeacherSnapshot>().id),
              Domain::TeacherId>);
static_assert(std::is_same_v<
              decltype(std::declval<ScheduleImportStateClassSnapshot>().id),
              Domain::ClassId>);
static_assert(std::is_same_v<
              decltype(std::declval<ScheduleImportStateClassSnapshot>().teacherId),
              Domain::TeacherId>);
static_assert(std::is_same_v<
              ScheduleImportStateClassReference,
              std::variant<
                  Domain::ClassId,
                  ScheduleImportStateCandidateIndex>>);
static_assert(
    ScheduleImportStateCandidateIndex{}
        == ScheduleImportStateCandidateIndex{0}
    );
static_assert(
    ScheduleImportStateCandidateIndex{3}.rawValue() == std::size_t(3)
    );
static_assert(std::is_constructible_v<
    ScheduleImportStateCandidateIndex,
    std::size_t
    >);
static_assert(!std::is_convertible_v<
    std::size_t,
    ScheduleImportStateCandidateIndex
    >);
static_assert(!std::is_convertible_v<
    ScheduleImportStateCandidateIndex,
    std::size_t
    >);
static_assert(std::is_same_v<
    decltype(ScheduleImportStateClassResolution{}.candidateIndex),
    ScheduleImportStateCandidateIndex
    >);
static_assert(
    ScheduleImportStateClassResolution{}.candidateIndex
        == ScheduleImportStateCandidateIndex{0}
    );

namespace
{
ReviewTeacherKey reviewTeacherKey(std::string value)
{
    return ReviewTeacherKey{std::move(value)};
}

Domain::TeacherId teacherId(int id)
{
    return Domain::TeacherId::fromString(std::to_string(id)).value();
}

Domain::ClassId classId(int id)
{
    return Domain::ClassId::fromString(std::to_string(id)).value();
}

ScheduleImportStateTime time(
    std::optional<Weekday> weekday,
    int start,
    int end,
    std::string dayLabel = "Monday",
    std::string startLabel = "4:00 PM",
    std::string endLabel = "4:55 PM"
    )
{
    return makeScheduleImportStateTime(
        weekday,
        start,
        end,
        std::move(dayLabel),
        std::move(startLabel),
        std::move(endLabel)
        );
}

ScheduleImportProjectedTime projectedTime(
    std::optional<Weekday> weekday,
    int start,
    int end,
    std::string dayLabel = "Monday",
    std::string startLabel = "4:00 PM",
    std::string endLabel = "4:55 PM"
    )
{
    const auto projected = projectScheduleImportStateTime(
        time(
            weekday,
            start,
            end,
            std::move(dayLabel),
            std::move(startLabel),
            std::move(endLabel)
            )
        );
    return projected.value();
}

ScheduleImportStateCandidate candidate(
    std::string teacherKey,
    std::string grade = "e5",
    std::string level = "zeus",
    std::vector<ScheduleImportStateTime> times = {}
    )
{
    return {
        reviewTeacherKey(std::move(teacherKey)),
        std::move(grade),
        std::move(level),
        "E5 Zeus",
        std::move(times)
    };
}

void addNewCandidate(
    ScheduleImportStateValidationRequest& request,
    const std::string& teacherKey,
    std::vector<ScheduleImportStateTime> times
    )
{
    const std::size_t candidateIndex = request.candidates.size();
    request.candidates.push_back(
        candidate(teacherKey, "e5", "level-" + std::to_string(candidateIndex),
                 std::move(times))
        );
    request.teacherResolutions.push_back(
        {
            reviewTeacherKey(teacherKey),
            ScheduleImportStateTeacherAction::Create,
            std::nullopt,
            true
        }
        );
    request.classResolutions.push_back(
        {
            ScheduleImportStateCandidateIndex{candidateIndex},
            ScheduleImportStateClassAction::CreateNew,
            std::nullopt
        }
        );
}

std::optional<ScheduleImportStateValidationErrorCode> errorCode(
    const ScheduleImportStateValidationRequest& request
    )
{
    const auto error = validateScheduleImportState(request);
    if (!error)
    {
        return std::nullopt;
    }
    return error->code;
}
}

class NextApplicationScheduleImportStateValidationTests final : public QObject
{
    Q_OBJECT

private slots:
    void sharedProjectionUsesHalfOpenTimesAndMatchingDays();
    void stateTimeStoresTypedValueAndKeepsLabels();
    void invalidStateTimeTuplesRemainAbsent();
    void sharedProjectionOrdersEveryConflictDeterministically();
    void rejectsStaleTeacherTargetsAndIdentity();
    void requiresSelectedTeacherAndAllowsAbsentActionTarget();
    void rejectsStaleClassTarget();
    void requiresSelectedClassAndAllowsUntargetedSkip();
    void rejectsCreateNewClassWithTarget();
    void requiresUniqueExactSkipTarget();
    void preservesArbitraryExactTeacherKeys();
    void rejectsSkippedClassWithMismatchedTeacherKey();
    void rejectsOutOfRangeCandidateIndexesForCreateAndUpdate();
    void rejectsUnknownStateKindBeforeProjectionConsumption();
    void rejectsUnknownIntensiveModeBeforeProjectionConsumption();
    void rejectsUnknownTeacherActionBeforeActionValidation();
    void rejectsUnknownClassActionBeforeIndexAndProjectionHandling();
    void acceptsReplaceModeForNormalState();
    void rejectsInvalidProjectedTimes();
    void rejectsOverlapsAndAllowsAdjacentTimes();
    void normalProjectionIncludesSkippedButDropsAbsentClasses();
    void intensiveProjectionPreservesAbsentOnlyInUpdateMode();
    void intensiveSkippedClassKeepsItsScheduleInBothModes();
    void projectedClassOrderRemainsNumericForTypedIds();
    void projectionUsesTypedReferencesAndPreservesMeetingOrder();
};

void NextApplicationScheduleImportStateValidationTests::
sharedProjectionUsesHalfOpenTimesAndMatchingDays()
{
    const auto first = ScheduleImportOverlapSchedule{
        "First",
        {projectedTime(Weekday::Monday, 16 * 60, 17 * 60)}
    };
    auto second = ScheduleImportOverlapSchedule{
        "Second",
        {projectedTime(Weekday::Monday, 17 * 60, 18 * 60)}
    };

    QVERIFY(projectScheduleImportOverlaps({first, second}).empty());

    second.times[0] = projectedTime(Weekday::Monday, 16 * 60 + 59, 18 * 60);
    auto conflicts = projectScheduleImportOverlaps({first, second});
    QCOMPARE(conflicts.size(), std::size_t(1));
    QCOMPARE(conflicts.front().classLabel, std::string("Second"));
    QCOMPARE(conflicts.front().conflictingClassLabel, std::string("First"));
    QVERIFY(
        conflicts.front().time.scheduleTime.weekday() == Weekday::Monday
        );
    QCOMPARE(conflicts.front().time.dayLabel, std::string("Monday"));

    second.times[0] = projectedTime(Weekday::Tuesday, 16 * 60, 18 * 60, "Tuesday");
    QVERIFY(projectScheduleImportOverlaps({first, second}).empty());
}

void NextApplicationScheduleImportStateValidationTests::
stateTimeStoresTypedValueAndKeepsLabels()
{
    const auto stateTime =
        time(Weekday::Sunday, 1438, 1439, "Sunday", "11:58 PM", "11:59 PM");
    QVERIFY(stateTime.scheduleTime.has_value());
    QVERIFY(stateTime.scheduleTime->weekday() == Weekday::Sunday);
    QCOMPARE(stateTime.scheduleTime->startMinute(), 1438);
    QCOMPARE(stateTime.scheduleTime->endMinute(), 1439);

    const auto projected = projectScheduleImportStateTime(stateTime);
    QVERIFY(projected.has_value());
    QVERIFY(projected->scheduleTime.weekday() == Weekday::Sunday);
    QCOMPARE(projected->scheduleTime.startMinute(), 1438);
    QCOMPARE(projected->scheduleTime.endMinute(), 1439);
    QCOMPARE(projected->dayLabel, std::string("Sunday"));
    QCOMPARE(projected->startLabel, std::string("11:58 PM"));
    QCOMPARE(projected->endLabel, std::string("11:59 PM"));

    QVERIFY(!projectScheduleImportStateTime(
                 time(std::nullopt, 1438, 1439, "Funday", "11:58 PM", "11:59 PM")
                 )
                 .has_value());
}

void NextApplicationScheduleImportStateValidationTests::
invalidStateTimeTuplesRemainAbsent()
{
    const std::vector<ScheduleImportStateTime> invalidTimes{
        time(std::nullopt, 16 * 60, 16 * 60 + 55, "Funday"),
        time(Weekday::Monday, -1, 16 * 60 + 55),
        time(Weekday::Monday, 16 * 60, -1),
        time(Weekday::Monday, 16 * 60, 16 * 60)
    };
    QCOMPARE(invalidTimes.front().dayLabel, std::string("Funday"));
    for (const auto& stateTime : invalidTimes)
    {
        QVERIFY(!stateTime.scheduleTime.has_value());
        QVERIFY(!projectScheduleImportStateTime(stateTime).has_value());
    }

    const ScheduleImportStateTime defaultStateTime;
    QVERIFY(!defaultStateTime.scheduleTime.has_value());
    QVERIFY(!projectScheduleImportStateTime(defaultStateTime).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
sharedProjectionOrdersEveryConflictDeterministically()
{
    const std::vector<ScheduleImportOverlapSchedule> schedules{
        {"A", {projectedTime(Weekday::Monday, 16 * 60, 18 * 60)}},
        {"B", {projectedTime(Weekday::Monday, 16 * 60 + 30, 17 * 60 + 30)}},
        {
            "C",
            {
                projectedTime(Weekday::Monday, 16 * 60 + 10, 16 * 60 + 20),
                projectedTime(Weekday::Monday, 17 * 60, 18 * 60 + 30)
            }
        }
    };

    const auto conflicts = projectScheduleImportOverlaps(schedules);
    QCOMPARE(conflicts.size(), std::size_t(4));
    QCOMPARE(conflicts[0].classLabel, std::string("B"));
    QCOMPARE(conflicts[0].conflictingClassLabel, std::string("A"));
    QCOMPARE(conflicts[0].time.scheduleTime.startMinute(), 16 * 60 + 30);
    QCOMPARE(conflicts[1].classLabel, std::string("C"));
    QCOMPARE(conflicts[1].conflictingClassLabel, std::string("A"));
    QCOMPARE(conflicts[1].time.scheduleTime.startMinute(), 16 * 60 + 10);
    QCOMPARE(conflicts[2].classLabel, std::string("C"));
    QCOMPARE(conflicts[2].conflictingClassLabel, std::string("A"));
    QCOMPARE(conflicts[2].time.scheduleTime.startMinute(), 17 * 60);
    QCOMPARE(conflicts[3].classLabel, std::string("C"));
    QCOMPARE(conflicts[3].conflictingClassLabel, std::string("B"));
    QCOMPARE(conflicts[3].time.scheduleTime.startMinute(), 17 * 60);

    const auto repeated = projectScheduleImportOverlaps(schedules);
    QCOMPARE(repeated.size(), conflicts.size());
    for (std::size_t index = 0; index < conflicts.size(); ++index)
    {
        QCOMPARE(repeated[index].classLabel, conflicts[index].classLabel);
        QCOMPARE(
            repeated[index].conflictingClassLabel,
            conflicts[index].conflictingClassLabel
            );
        QCOMPARE(
            repeated[index].time.scheduleTime.startMinute(),
            conflicts[index].time.scheduleTime.startMinute()
            );
    }
}

void NextApplicationScheduleImportStateValidationTests::
rejectsStaleTeacherTargetsAndIdentity()
{
    ScheduleImportStateValidationRequest request;
    request.existingTeachers = {{teacherId(7), reviewTeacherKey("teacher-a")}};
    request.teacherResolutions = {
        {
            reviewTeacherKey("teacher-a"),
            ScheduleImportStateTeacherAction::Reuse,
            teacherId(8),
            true
        }
    };
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::SelectedTeacherUnavailable
        }
        );

    request.teacherResolutions[0].targetTeacherId = teacherId(7);
    request.teacherResolutions[0].teacherKey = reviewTeacherKey("teacher-b");
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::SelectedTeacherUnavailable
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
requiresSelectedTeacherAndAllowsAbsentActionTarget()
{
    ScheduleImportStateValidationRequest request;
    request.existingTeachers = {{teacherId(7), reviewTeacherKey("teacher-a")}};
    request.teacherResolutions = {
        {
            reviewTeacherKey("teacher-a"),
            ScheduleImportStateTeacherAction::Reuse,
            teacherId(7),
            true
        }
    };
    QVERIFY(!errorCode(request).has_value());

    request.teacherResolutions = {
        {
            reviewTeacherKey("teacher-a"),
            ScheduleImportStateTeacherAction::UpdateRoom,
            teacherId(7),
            true
        }
    };
    QVERIFY(!errorCode(request).has_value());

    request.teacherResolutions[0].targetTeacherId = std::nullopt;
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::SelectedTeacherUnavailable
        }
        );

    request.teacherResolutions = {
        {
            reviewTeacherKey("teacher-a"),
            ScheduleImportStateTeacherAction::Create,
            std::nullopt,
            true
        },
        {
            reviewTeacherKey("teacher-b"),
            ScheduleImportStateTeacherAction::Skip,
            std::nullopt,
            false
        }
    };
    QVERIFY(!errorCode(request).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
rejectsStaleClassTarget()
{
    ScheduleImportStateValidationRequest request;
    request.candidates = {candidate("teacher-a")};
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::UpdateExisting,
            classId(42)
        }
    };
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::SelectedClassUnavailable
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
requiresSelectedClassAndAllowsUntargetedSkip()
{
    ScheduleImportStateValidationRequest request;
    request.candidates = {candidate("teacher-a")};
    request.existingClasses = {
        {classId(42), teacherId(3), "e5", "zeus", "E5 Zeus", {}, {}}
    };
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::UpdateExisting,
            classId(42)
        }
    };
    QVERIFY(!errorCode(request).has_value());

    request.classResolutions[0].targetClassId = std::nullopt;
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::SelectedClassUnavailable
        }
        );

    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::Skip,
            std::nullopt
        }
    };
    QVERIFY(!errorCode(request).has_value());

    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{99},
            ScheduleImportStateClassAction::Skip,
            classId(42)
        }
    };
    QVERIFY(!errorCode(request).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
rejectsCreateNewClassWithTarget()
{
    ScheduleImportStateValidationRequest request;
    request.candidates = {candidate("teacher-a")};
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::CreateNew,
            classId(42)
        }
    };

    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::CreateNewClassHasTarget
        }
        );

    request.classResolutions[0].targetClassId = std::nullopt;
    QVERIFY(!errorCode(request).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
requiresUniqueExactSkipTarget()
{
    ScheduleImportStateValidationRequest request;
    request.existingTeachers = {{teacherId(3), reviewTeacherKey("teacher-a")}};
    request.candidates = {candidate("teacher-a")};
    request.teacherResolutions = {
        {
            reviewTeacherKey("teacher-a"),
            ScheduleImportStateTeacherAction::Reuse,
            teacherId(3),
            true
        }
    };
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::Skip,
            classId(11)
        }
    };
    request.existingClasses = {
        {classId(11), teacherId(3), "e5", "zeus", "E5 Zeus", {}, {}},
        {classId(12), teacherId(3), "e5", "zeus", "E5 Zeus", {}, {}}
    };
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::
                SkippedClassNotUniqueExactMatch
        }
        );

    request.existingClasses.pop_back();
    QVERIFY(!errorCode(request).has_value());

    request.classResolutions[0].targetClassId = classId(99);
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::
                SkippedClassNotUniqueExactMatch
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
preservesArbitraryExactTeacherKeys()
{
    const std::string composed = "teacher-\xC3\xA9-\xF0\x9F\xA7\xAA";
    const std::string decomposed = "teacher-e\xCC\x81-\xF0\x9F\xA7\xAA";
    const ReviewTeacherKey composedKey{composed};
    const ReviewTeacherKey decomposedKey{decomposed};
    QVERIFY(composedKey.value() == composed);
    QVERIFY(decomposedKey.value() == decomposed);
    QVERIFY(composedKey != decomposedKey);

    ScheduleImportStateValidationRequest request;
    request.existingTeachers = {{teacherId(3), composedKey}};
    request.candidates = {candidate(composed, "e5", "zeus")};
    request.teacherResolutions = {
        {
            composedKey,
            ScheduleImportStateTeacherAction::Reuse,
            teacherId(3),
            true
        }
    };
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::Skip,
            classId(11)
        }
    };
    request.existingClasses = {
        {classId(11), teacherId(3), "e5", "zeus", "E5 Zeus", {}, {}}
    };

    QVERIFY(!errorCode(request).has_value());

    request.teacherResolutions[0].teacherKey = decomposedKey;
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::SelectedTeacherUnavailable
        }
        );

    request.teacherResolutions[0].teacherKey = composedKey;
    request.candidates[0].teacherKey = decomposedKey;
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::
                SkippedClassNotUniqueExactMatch
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
rejectsSkippedClassWithMismatchedTeacherKey()
{
    ScheduleImportStateValidationRequest request;
    request.existingTeachers = {{teacherId(3), reviewTeacherKey("teacher-b")}};
    request.candidates = {candidate("teacher-a", "e5", "zeus")};
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::Skip,
            classId(11)
        }
    };
    request.existingClasses = {
        {classId(11), teacherId(3), "e5", "zeus", "E5 Zeus", {}, {}}
    };

    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::
                SkippedClassNotUniqueExactMatch
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
rejectsOutOfRangeCandidateIndexesForCreateAndUpdate()
{
    ScheduleImportStateValidationRequest request;
    request.existingClasses = {
        {
            classId(42),
            teacherId(3),
            "e5",
            "zeus",
            "E5 Zeus",
            {},
            {}
        }
    };

    for (const auto action : {
             ScheduleImportStateClassAction::CreateNew,
             ScheduleImportStateClassAction::UpdateExisting
         })
    {
        request.classResolutions = {
            {
                ScheduleImportStateCandidateIndex{0},
                action,
                action == ScheduleImportStateClassAction::UpdateExisting
                    ? std::optional<Domain::ClassId>{classId(42)}
                    : std::nullopt
            }
        };

        const auto error = validateScheduleImportState(request);
        QVERIFY(error.has_value());
        QCOMPARE(
            error->code,
            ScheduleImportStateValidationErrorCode::InvalidClassCandidateIndex
            );

        const auto suppliedProjectionError = validateScheduleImportState(
            request,
            std::vector<ScheduleImportStateProjectedSchedule>{}
            );
        QVERIFY(suppliedProjectionError.has_value());
        QCOMPARE(
            suppliedProjectionError->code,
            ScheduleImportStateValidationErrorCode::
                InvalidClassCandidateIndex
            );
    }
}

void NextApplicationScheduleImportStateValidationTests::
rejectsUnknownStateKindBeforeProjectionConsumption()
{
    ScheduleImportStateValidationRequest request;
    request.kind = static_cast<ScheduleImportStateKind>(99);

    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::InvalidStateKind
        }
        );

    const ScheduleImportStateProjectedSchedule invalidProjection{
        ScheduleImportStateCandidateIndex{0},
        "Invalid projection",
        {time(std::nullopt, 16 * 60, 17 * 60, "Funday")}
    };
    const auto suppliedProjectionError = validateScheduleImportState(
        request,
        std::vector<ScheduleImportStateProjectedSchedule>{invalidProjection}
        );
    QVERIFY(suppliedProjectionError.has_value());
    QCOMPARE(
        suppliedProjectionError->code,
        ScheduleImportStateValidationErrorCode::InvalidStateKind
        );
}

void NextApplicationScheduleImportStateValidationTests::
rejectsUnknownIntensiveModeBeforeProjectionConsumption()
{
    ScheduleImportStateValidationRequest request;
    request.intensiveMode =
        static_cast<ScheduleImportStateIntensiveMode>(99);

    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::InvalidIntensiveMode
        }
        );

    const ScheduleImportStateProjectedSchedule invalidProjection{
        ScheduleImportStateCandidateIndex{0},
        "Invalid projection",
        {time(std::nullopt, 16 * 60, 17 * 60, "Funday")}
    };
    const auto suppliedProjectionError = validateScheduleImportState(
        request,
        std::vector<ScheduleImportStateProjectedSchedule>{invalidProjection}
        );
    QVERIFY(suppliedProjectionError.has_value());
    QCOMPARE(
        suppliedProjectionError->code,
        ScheduleImportStateValidationErrorCode::InvalidIntensiveMode
        );
}

void NextApplicationScheduleImportStateValidationTests::
rejectsUnknownTeacherActionBeforeActionValidation()
{
    ScheduleImportStateValidationRequest request;
    request.teacherResolutions = {
        {
            reviewTeacherKey("teacher-a"),
            static_cast<ScheduleImportStateTeacherAction>(99),
            teacherId(7),
            false
        }
    };

    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::InvalidTeacherAction
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
rejectsUnknownClassActionBeforeIndexAndProjectionHandling()
{
    ScheduleImportStateValidationRequest request;
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{7},
            static_cast<ScheduleImportStateClassAction>(99),
            std::nullopt
        }
    };

    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::InvalidClassAction
        }
        );

    const ScheduleImportStateProjectedSchedule invalidProjection{
        ScheduleImportStateCandidateIndex{0},
        "Invalid projection",
        {time(std::nullopt, 16 * 60, 17 * 60, "Funday")}
    };
    const auto suppliedProjectionError = validateScheduleImportState(
        request,
        std::vector<ScheduleImportStateProjectedSchedule>{invalidProjection}
        );
    QVERIFY(suppliedProjectionError.has_value());
    QCOMPARE(
        suppliedProjectionError->code,
        ScheduleImportStateValidationErrorCode::InvalidClassAction
        );
}

void NextApplicationScheduleImportStateValidationTests::
acceptsReplaceModeForNormalState()
{
    ScheduleImportStateValidationRequest request;
    request.kind = ScheduleImportStateKind::Normal;
    request.intensiveMode = ScheduleImportStateIntensiveMode::ReplaceWithNew;

    QVERIFY(!validateScheduleImportState(request).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
rejectsInvalidProjectedTimes()
{
    const std::vector<ScheduleImportStateTime> invalidTimes{
        time(std::nullopt, 16 * 60, 16 * 60 + 55, "Funday"),
        time(Weekday::Monday, -1, 16 * 60 + 55, "Monday", "unparsed", "4:55 PM"),
        time(Weekday::Monday, 16 * 60, -1, "Monday", "4:00 PM", "unparsed"),
        time(Weekday::Monday, 16 * 60, 16 * 60, "Monday", "4:00 PM", "4:00 PM")
    };
    for (const auto& invalidTime : invalidTimes)
    {
        ScheduleImportStateValidationRequest request;
        addNewCandidate(request, "teacher-a", {invalidTime});
        const auto error = validateScheduleImportState(request);
        QVERIFY(error.has_value());
        QCOMPARE(
            error->code,
            ScheduleImportStateValidationErrorCode::InvalidProjectedTime
            );
        QCOMPARE(error->classLabel, std::string("E5 Zeus"));
        QCOMPARE(error->day, invalidTime.dayLabel);
        QCOMPARE(error->startTime, invalidTime.startLabel);
        QCOMPARE(error->endTime, invalidTime.endLabel);
    }

    ScheduleImportStateValidationRequest defaultStateRequest;
    addNewCandidate(
        defaultStateRequest,
        "teacher-default",
        {ScheduleImportStateTime{}}
        );
    const auto defaultError = validateScheduleImportState(defaultStateRequest);
    QVERIFY(defaultError.has_value());
    QCOMPARE(
        defaultError->code,
        ScheduleImportStateValidationErrorCode::InvalidProjectedTime
        );
    QCOMPARE(defaultError->classLabel, std::string("E5 Zeus"));
    QVERIFY(defaultError->day.empty());
    QVERIFY(defaultError->startTime.empty());
    QVERIFY(defaultError->endTime.empty());

    ScheduleImportStateValidationRequest overlapPrecedenceRequest;
    addNewCandidate(
        overlapPrecedenceRequest,
        "teacher-a",
        {time(Weekday::Monday, 16 * 60, 17 * 60)}
        );
    addNewCandidate(
        overlapPrecedenceRequest,
        "teacher-b",
        {time(Weekday::Monday, 16 * 60 + 30, 17 * 60 + 30)}
        );
    addNewCandidate(
        overlapPrecedenceRequest,
        "teacher-c",
        {time(Weekday::Monday, -1, 16 * 60 + 55, "Monday", "unparsed", "4:55 PM")}
        );
    const auto precedenceError =
        validateScheduleImportState(overlapPrecedenceRequest);
    QVERIFY(precedenceError.has_value());
    QCOMPARE(
        precedenceError->code,
        ScheduleImportStateValidationErrorCode::InvalidProjectedTime
        );
    QCOMPARE(precedenceError->classLabel, std::string("E5 Zeus"));
    QCOMPARE(precedenceError->day, std::string("Monday"));
    QCOMPARE(precedenceError->startTime, std::string("unparsed"));
    QCOMPARE(precedenceError->endTime, std::string("4:55 PM"));
}

void NextApplicationScheduleImportStateValidationTests::
rejectsOverlapsAndAllowsAdjacentTimes()
{
    ScheduleImportStateValidationRequest request;
    addNewCandidate(
        request,
        "teacher-a",
        {time(Weekday::Monday, 16 * 60, 16 * 60 + 55)}
        );
    addNewCandidate(
        request,
        "teacher-b",
        {time(Weekday::Monday, 16 * 60 + 54, 17 * 60)}
        );
    request.candidates[0].classLabel = "First";
    request.candidates[1].classLabel = "Second";
    const auto conflict = validateScheduleImportState(request);
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
        }
        );
    QVERIFY(conflict.has_value());
    QCOMPARE(conflict->classLabel, std::string("First"));
    QCOMPARE(conflict->conflictingClassLabel, std::string("Second"));

    request.candidates[1].times[0] = time(Weekday::Monday, 16 * 60 + 55, 17 * 60);
    QVERIFY(!errorCode(request).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
normalProjectionIncludesSkippedButDropsAbsentClasses()
{
    ScheduleImportStateValidationRequest request;
    request.kind = ScheduleImportStateKind::Normal;
    request.existingTeachers = {{teacherId(3), reviewTeacherKey("teacher-a")}};
    request.existingClasses = {
        {
            classId(11),
            teacherId(3),
            "e5",
            "zeus",
            "E5 Zeus",
            {time(Weekday::Monday, 16 * 60, 17 * 60)},
            {}
        }
    };
    addNewCandidate(
        request,
        "teacher-b",
        {time(Weekday::Monday, 16 * 60, 17 * 60)}
        );
    // Normal replacement drops unrelated existing classes from projection.
    QVERIFY(!errorCode(request).has_value());

    request.candidates.insert(request.candidates.begin(), candidate("teacher-a"));
    request.teacherResolutions.insert(
        request.teacherResolutions.begin(),
        {
            reviewTeacherKey("teacher-a"),
            ScheduleImportStateTeacherAction::Reuse,
            teacherId(3),
            true
        }
        );
    request.classResolutions.insert(
        request.classResolutions.begin(),
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::Skip,
            classId(11)
        }
        );
    request.classResolutions[1].candidateIndex =
        ScheduleImportStateCandidateIndex{1};
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
intensiveProjectionPreservesAbsentOnlyInUpdateMode()
{
    ScheduleImportStateValidationRequest request;
    request.kind = ScheduleImportStateKind::Intensive;
    request.intensiveMode = ScheduleImportStateIntensiveMode::UpdateExisting;
    request.existingClasses = {
        {
            classId(11),
            teacherId(3),
            "e5",
            "apollo",
            "E5 Apollo",
            {},
            {time(Weekday::Monday, 16 * 60, 17 * 60)}
        }
    };
    addNewCandidate(
        request,
        "teacher-b",
        {time(Weekday::Monday, 16 * 60, 17 * 60)}
        );
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
        }
        );
    const auto updateProjection =
        projectScheduleImportStateSchedules(request);
    QCOMPARE(updateProjection.size(), std::size_t(2));
    QCOMPARE(
        updateProjection[1].persistence,
        ScheduleImportStateProjectedSchedule::Persistence::KeepExistingRows
        );

    request.intensiveMode = ScheduleImportStateIntensiveMode::ReplaceWithNew;
    QVERIFY(!errorCode(request).has_value());
    const auto replacementProjection =
        projectScheduleImportStateSchedules(request);
    QCOMPARE(replacementProjection.size(), std::size_t(1));

    request.intensiveMode = ScheduleImportStateIntensiveMode::UpdateExisting;
    request.existingClasses[0].intensiveTimes = {
        time(Weekday::Monday, 17 * 60, 18 * 60)
    };
    QVERIFY(!errorCode(request).has_value());
    const auto updatedProjection =
        projectScheduleImportStateSchedules(request);
    QCOMPARE(updatedProjection.size(), std::size_t(2));
    QCOMPARE(
        updatedProjection[1].persistence,
        ScheduleImportStateProjectedSchedule::Persistence::KeepExistingRows
        );
}

void NextApplicationScheduleImportStateValidationTests::
projectedClassOrderRemainsNumericForTypedIds()
{
    ScheduleImportStateValidationRequest request;
    request.kind = ScheduleImportStateKind::Intensive;
    request.intensiveMode = ScheduleImportStateIntensiveMode::UpdateExisting;
    request.existingClasses = {
        {
            classId(10),
            teacherId(3),
            "e5",
            "ten",
            "ID 10",
            {},
            {time(Weekday::Monday, 16 * 60, 17 * 60)}
        },
        {
            classId(2),
            teacherId(3),
            "e5",
            "two",
            "ID 2",
            {},
            {time(Weekday::Monday, 16 * 60, 17 * 60)}
        }
    };

    const auto error = validateScheduleImportState(request);
    QVERIFY(error.has_value());
    QCOMPARE(
        error->code,
        ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
        );
    QCOMPARE(error->classLabel, std::string("ID 10"));
    QCOMPARE(error->conflictingClassLabel, std::string("ID 2"));
}

void NextApplicationScheduleImportStateValidationTests::
intensiveSkippedClassKeepsItsScheduleInBothModes()
{
    ScheduleImportStateValidationRequest request;
    request.kind = ScheduleImportStateKind::Intensive;
    request.existingTeachers = {{teacherId(3), reviewTeacherKey("teacher-a")}};
    request.existingClasses = {
        {
            classId(11),
            teacherId(3),
            "e5",
            "zeus",
            "E5 Zeus",
            {},
            {time(Weekday::Monday, 16 * 60, 17 * 60)}
        }
    };
    request.candidates = {
        candidate("teacher-a", "e5", "zeus")
    };
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::Skip,
            classId(11)
        }
    };
    addNewCandidate(
        request,
        "teacher-b",
        {time(Weekday::Monday, 16 * 60 + 30, 17 * 60 + 30)}
        );

    request.intensiveMode = ScheduleImportStateIntensiveMode::UpdateExisting;
    auto updateProjection = projectScheduleImportStateSchedules(request);
    QCOMPARE(updateProjection.size(), std::size_t(2));
    QCOMPARE(
        updateProjection[1].persistence,
        ScheduleImportStateProjectedSchedule::Persistence::KeepExistingRows
        );
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
        }
        );

    request.intensiveMode = ScheduleImportStateIntensiveMode::ReplaceWithNew;
    auto replaceProjection = projectScheduleImportStateSchedules(request);
    QCOMPARE(replaceProjection.size(), std::size_t(2));
    QCOMPARE(
        replaceProjection[1].persistence,
        ScheduleImportStateProjectedSchedule::Persistence::ReplaceRows
        );
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
projectionUsesTypedReferencesAndPreservesMeetingOrder()
{
    ScheduleImportStateValidationRequest request;
    request.existingClasses = {
        {
            classId(42),
            teacherId(3),
            "e5",
            "zeus",
            "E5 Zeus",
            {},
            {}
        }
    };
    request.candidates = {
        candidate(
            "teacher-new",
            "m2",
            "atlas",
            {
                time(Weekday::Monday, 16 * 60, 16 * 60 + 55),
                time(Weekday::Friday, 17 * 60, 17 * 60 + 55, "Friday", "5:00 PM", "5:55 PM")
            }
            ),
        candidate(
            "teacher-second-new",
            "e5",
            "zeus",
            {
                time(Weekday::Tuesday, 18 * 60, 18 * 60 + 55, "Tuesday", "6:00 PM", "6:55 PM"),
                time(Weekday::Thursday, 19 * 60, 19 * 60 + 55, "Thursday", "7:00 PM", "7:55 PM")
            }
            ),
        candidate(
            "teacher-existing",
            "m3",
            "orion",
            {
                time(Weekday::Wednesday, 20 * 60, 20 * 60 + 55, "Wednesday", "8:00 PM", "8:55 PM")
            }
            )
    };
    request.candidates[0].classLabel = "Candidate zero";
    request.candidates[1].classLabel = "Candidate one";
    request.candidates[2].classLabel = "Candidate two";
    request.classResolutions = {
        {
            ScheduleImportStateCandidateIndex{0},
            ScheduleImportStateClassAction::CreateNew,
            std::nullopt
        },
        {
            ScheduleImportStateCandidateIndex{1},
            ScheduleImportStateClassAction::CreateNew,
            std::nullopt
        },
        {
            ScheduleImportStateCandidateIndex{2},
            ScheduleImportStateClassAction::UpdateExisting,
            classId(42)
        }
    };

    const auto projected = projectScheduleImportStateSchedules(request);
    QCOMPARE(projected.size(), std::size_t(3));
    QVERIFY(std::holds_alternative<ScheduleImportStateCandidateIndex>(
        projected[0].classReference
        ));
    QCOMPARE(
        std::get<ScheduleImportStateCandidateIndex>(
            projected[0].classReference
            ).rawValue(),
        std::size_t(1)
        );
    QVERIFY(std::holds_alternative<ScheduleImportStateCandidateIndex>(
        projected[1].classReference
        ));
    QCOMPARE(
        std::get<ScheduleImportStateCandidateIndex>(
            projected[1].classReference
            ).rawValue(),
        std::size_t(0)
        );
    QVERIFY(std::holds_alternative<Domain::ClassId>(
        projected[2].classReference
        ));
    QCOMPARE(
        std::get<Domain::ClassId>(projected[2].classReference),
        classId(42)
        );
    QCOMPARE(projected[0].classLabel, std::string("Candidate one"));
    QCOMPARE(projected[1].classLabel, std::string("Candidate zero"));
    QCOMPARE(projected[2].classLabel, std::string("Candidate two"));
    QCOMPARE(projected[0].times.size(), std::size_t(2));
    QCOMPARE(projected[0].times[0].dayLabel, std::string("Tuesday"));
    QCOMPARE(projected[0].times[1].dayLabel, std::string("Thursday"));
    QCOMPARE(projected[1].times.size(), std::size_t(2));
    QCOMPARE(projected[1].times[0].dayLabel, std::string("Monday"));
    QCOMPARE(projected[1].times[1].dayLabel, std::string("Friday"));
    QCOMPARE(projected[2].times.size(), std::size_t(1));
    QCOMPARE(projected[2].times[0].dayLabel, std::string("Wednesday"));
    QVERIFY(!validateScheduleImportState(request).has_value());

    request.candidates[1].times[0] =
        time(Weekday::Monday, 16 * 60 + 30, 17 * 60 + 30);
    const auto overlapping = projectScheduleImportStateSchedules(request);
    const auto error = validateScheduleImportState(request, overlapping);
    QVERIFY(error.has_value());
    QCOMPARE(
        error->code,
        ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
        );
}

QTEST_APPLESS_MAIN(NextApplicationScheduleImportStateValidationTests)

#include "next_application_schedule_import_state_validation_tests.moc"
