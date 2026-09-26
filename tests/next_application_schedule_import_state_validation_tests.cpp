#include "next/application/schedule_import_state_validation.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;
using ClassMngr::Next::Domain::Weekday;
namespace Domain = ClassMngr::Next::Domain;

static_assert(!std::is_convertible_v<Domain::TeacherId, Domain::ClassId>);
static_assert(!std::is_convertible_v<Domain::ClassId, Domain::TeacherId>);
static_assert(!std::is_assignable_v<Domain::TeacherId&, Domain::ClassId>);
static_assert(!std::is_assignable_v<Domain::ClassId&, Domain::TeacherId>);
static_assert(std::is_same_v<
              decltype(ScheduleImportStateTeacherResolution{}.targetTeacherId),
              std::optional<Domain::TeacherId>>);
static_assert(std::is_same_v<
              decltype(ScheduleImportStateClassResolution{}.targetClassId),
              std::optional<Domain::ClassId>>);
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

namespace
{
Domain::TeacherId teacherId(int id)
{
    return Domain::TeacherId::fromString(std::to_string(id)).value();
}

Domain::ClassId classId(int id)
{
    return Domain::ClassId::fromString(std::to_string(id)).value();
}

ScheduleImportStateTime time(
    int day,
    int start,
    int end,
    std::string dayLabel = "Monday",
    std::string startLabel = "4:00 PM",
    std::string endLabel = "4:55 PM"
    )
{
    return {
        day,
        start,
        end,
        std::move(dayLabel),
        std::move(startLabel),
        std::move(endLabel)
    };
}

ScheduleImportProjectedTime projectedTime(
    int day,
    int start,
    int end,
    std::string dayLabel = "Monday",
    std::string startLabel = "4:00 PM",
    std::string endLabel = "4:55 PM"
    )
{
    const auto projected = projectScheduleImportStateTime(
        time(
            day,
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
        std::move(teacherKey),
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
        {teacherKey, ScheduleImportStateTeacherAction::Create, std::nullopt, true}
        );
    request.classResolutions.push_back(
        {
            candidateIndex,
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
    void rawStateTimeProjectsToDomainValueAndKeepsLabels();
    void sharedProjectionOrdersEveryConflictDeterministically();
    void rejectsStaleTeacherTargetsAndIdentity();
    void requiresSelectedTeacherAndAllowsAbsentActionTarget();
    void rejectsStaleClassTarget();
    void requiresSelectedClassAndAllowsUntargetedSkip();
    void rejectsCreateNewClassWithTarget();
    void requiresUniqueExactSkipTarget();
    void rejectsSkippedClassWithMismatchedTeacherKey();
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
        {projectedTime(0, 16 * 60, 17 * 60)}
    };
    auto second = ScheduleImportOverlapSchedule{
        "Second",
        {projectedTime(0, 17 * 60, 18 * 60)}
    };

    QVERIFY(projectScheduleImportOverlaps({first, second}).empty());

    second.times[0] = projectedTime(0, 16 * 60 + 59, 18 * 60);
    auto conflicts = projectScheduleImportOverlaps({first, second});
    QCOMPARE(conflicts.size(), std::size_t(1));
    QCOMPARE(conflicts.front().classLabel, std::string("Second"));
    QCOMPARE(conflicts.front().conflictingClassLabel, std::string("First"));
    QVERIFY(
        conflicts.front().time.scheduleTime.weekday() == Weekday::Monday
        );
    QCOMPARE(conflicts.front().time.dayLabel, std::string("Monday"));

    second.times[0] = projectedTime(1, 16 * 60, 18 * 60, "Tuesday");
    QVERIFY(projectScheduleImportOverlaps({first, second}).empty());
}

void NextApplicationScheduleImportStateValidationTests::
rawStateTimeProjectsToDomainValueAndKeepsLabels()
{
    const auto projected = projectScheduleImportStateTime(
        time(6, 1438, 1439, "Sunday", "11:58 PM", "11:59 PM")
        );
    QVERIFY(projected.has_value());
    QVERIFY(projected->scheduleTime.weekday() == Weekday::Sunday);
    QCOMPARE(projected->scheduleTime.weekdayIndex(), 6);
    QCOMPARE(projected->scheduleTime.startMinute(), 1438);
    QCOMPARE(projected->scheduleTime.endMinute(), 1439);
    QCOMPARE(projected->dayLabel, std::string("Sunday"));
    QCOMPARE(projected->startLabel, std::string("11:58 PM"));
    QCOMPARE(projected->endLabel, std::string("11:59 PM"));

    QVERIFY(!projectScheduleImportStateTime(
                 time(7, 1438, 1439, "Funday", "11:58 PM", "11:59 PM")
                 )
                 .has_value());
}

void NextApplicationScheduleImportStateValidationTests::
sharedProjectionOrdersEveryConflictDeterministically()
{
    const std::vector<ScheduleImportOverlapSchedule> schedules{
        {"A", {projectedTime(0, 16 * 60, 18 * 60)}},
        {"B", {projectedTime(0, 16 * 60 + 30, 17 * 60 + 30)}},
        {
            "C",
            {
                projectedTime(0, 16 * 60 + 10, 16 * 60 + 20),
                projectedTime(0, 17 * 60, 18 * 60 + 30)
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
    request.existingTeachers = {{teacherId(7), "teacher-a"}};
    request.teacherResolutions = {
        {"teacher-a", ScheduleImportStateTeacherAction::Reuse, teacherId(8), true}
    };
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::SelectedTeacherUnavailable
        }
        );

    request.teacherResolutions[0].targetTeacherId = teacherId(7);
    request.teacherResolutions[0].teacherKey = "teacher-b";
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
    request.existingTeachers = {{teacherId(7), "teacher-a"}};
    request.teacherResolutions = {
        {"teacher-a", ScheduleImportStateTeacherAction::Reuse, teacherId(7), true}
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
        {"teacher-a", ScheduleImportStateTeacherAction::Create, std::nullopt, true},
        {"teacher-b", ScheduleImportStateTeacherAction::Skip, std::nullopt, false}
    };
    QVERIFY(!errorCode(request).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
rejectsStaleClassTarget()
{
    ScheduleImportStateValidationRequest request;
    request.candidates = {candidate("teacher-a")};
    request.classResolutions = {
        {0, ScheduleImportStateClassAction::UpdateExisting, classId(42)}
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
        {0, ScheduleImportStateClassAction::UpdateExisting, classId(42)}
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
        {0, ScheduleImportStateClassAction::Skip, std::nullopt}
    };
    QVERIFY(!errorCode(request).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
rejectsCreateNewClassWithTarget()
{
    ScheduleImportStateValidationRequest request;
    request.candidates = {candidate("teacher-a")};
    request.classResolutions = {
        {0, ScheduleImportStateClassAction::CreateNew, classId(42)}
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
    request.existingTeachers = {{teacherId(3), "teacher-a"}};
    request.candidates = {candidate("teacher-a")};
    request.teacherResolutions = {
        {"teacher-a", ScheduleImportStateTeacherAction::Reuse, teacherId(3), true}
    };
    request.classResolutions = {
        {0, ScheduleImportStateClassAction::Skip, classId(11)}
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
rejectsSkippedClassWithMismatchedTeacherKey()
{
    ScheduleImportStateValidationRequest request;
    request.existingTeachers = {{teacherId(3), "teacher-b"}};
    request.candidates = {candidate("teacher-a", "e5", "zeus")};
    request.classResolutions = {
        {0, ScheduleImportStateClassAction::Skip, classId(11)}
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
rejectsInvalidProjectedTimes()
{
    ScheduleImportStateValidationRequest request;
    addNewCandidate(
        request,
        "teacher-a",
        {time(-1, 16 * 60, 16 * 60 + 55, "Funday")}
        );
    const auto error = validateScheduleImportState(request);
    QVERIFY(error.has_value());
    QCOMPARE(
        error->code,
        ScheduleImportStateValidationErrorCode::InvalidProjectedTime
        );
    QCOMPARE(error->classLabel, std::string("E5 Zeus"));
    QCOMPARE(error->day, std::string("Funday"));
    QCOMPARE(error->startTime, std::string("4:00 PM"));
    QCOMPARE(error->endTime, std::string("4:55 PM"));

    request.candidates[0].times[0] = time(0, 16 * 60, 16 * 60);
    QCOMPARE(
        errorCode(request),
        std::optional{
            ScheduleImportStateValidationErrorCode::InvalidProjectedTime
        }
        );
}

void NextApplicationScheduleImportStateValidationTests::
rejectsOverlapsAndAllowsAdjacentTimes()
{
    ScheduleImportStateValidationRequest request;
    addNewCandidate(
        request,
        "teacher-a",
        {time(0, 16 * 60, 16 * 60 + 55)}
        );
    addNewCandidate(
        request,
        "teacher-b",
        {time(0, 16 * 60 + 54, 17 * 60)}
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

    request.candidates[1].times[0] = time(0, 16 * 60 + 55, 17 * 60);
    QVERIFY(!errorCode(request).has_value());
}

void NextApplicationScheduleImportStateValidationTests::
normalProjectionIncludesSkippedButDropsAbsentClasses()
{
    ScheduleImportStateValidationRequest request;
    request.kind = ScheduleImportStateKind::Normal;
    request.existingTeachers = {{teacherId(3), "teacher-a"}};
    request.existingClasses = {
        {
            classId(11),
            teacherId(3),
            "e5",
            "zeus",
            "E5 Zeus",
            {time(0, 16 * 60, 17 * 60)},
            {}
        }
    };
    addNewCandidate(
        request,
        "teacher-b",
        {time(0, 16 * 60, 17 * 60)}
        );
    // Normal replacement drops unrelated existing classes from projection.
    QVERIFY(!errorCode(request).has_value());

    request.candidates.insert(request.candidates.begin(), candidate("teacher-a"));
    request.teacherResolutions.insert(
        request.teacherResolutions.begin(),
        {"teacher-a", ScheduleImportStateTeacherAction::Reuse, teacherId(3), true}
        );
    request.classResolutions.insert(
        request.classResolutions.begin(),
        {0, ScheduleImportStateClassAction::Skip, classId(11)}
        );
    request.classResolutions[1].candidateIndex = 1;
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
            {time(0, 16 * 60, 17 * 60)}
        }
    };
    addNewCandidate(
        request,
        "teacher-b",
        {time(0, 16 * 60, 17 * 60)}
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
        time(0, 17 * 60, 18 * 60)
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
            {time(0, 16 * 60, 17 * 60)}
        },
        {
            classId(2),
            teacherId(3),
            "e5",
            "two",
            "ID 2",
            {},
            {time(0, 16 * 60, 17 * 60)}
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
    request.existingTeachers = {{teacherId(3), "teacher-a"}};
    request.existingClasses = {
        {
            classId(11),
            teacherId(3),
            "e5",
            "zeus",
            "E5 Zeus",
            {},
            {time(0, 16 * 60, 17 * 60)}
        }
    };
    request.candidates = {
        candidate("teacher-a", "e5", "zeus")
    };
    request.classResolutions = {
        {0, ScheduleImportStateClassAction::Skip, classId(11)}
    };
    addNewCandidate(
        request,
        "teacher-b",
        {time(0, 16 * 60 + 30, 17 * 60 + 30)}
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
                time(0, 16 * 60, 16 * 60 + 55),
                time(4, 17 * 60, 17 * 60 + 55, "Friday", "5:00 PM", "5:55 PM")
            }
            ),
        candidate(
            "teacher-existing",
            "e5",
            "zeus",
            {
                time(1, 18 * 60, 18 * 60 + 55, "Tuesday", "6:00 PM", "6:55 PM"),
                time(3, 19 * 60, 19 * 60 + 55, "Thursday", "7:00 PM", "7:55 PM")
            }
            )
    };
    request.classResolutions = {
        {0, ScheduleImportStateClassAction::CreateNew, std::nullopt},
        {1, ScheduleImportStateClassAction::UpdateExisting, classId(42)}
    };

    const auto projected = projectScheduleImportStateSchedules(request);
    QCOMPARE(projected.size(), std::size_t(2));
    QVERIFY(std::holds_alternative<ScheduleImportStateCandidateIndex>(
        projected[0].classReference
        ));
    QCOMPARE(
        std::get<ScheduleImportStateCandidateIndex>(
            projected[0].classReference
            ).value,
        std::size_t(0)
        );
    QVERIFY(std::holds_alternative<Domain::ClassId>(
        projected[1].classReference
        ));
    QCOMPARE(
        std::get<Domain::ClassId>(projected[1].classReference),
        classId(42)
        );
    QCOMPARE(projected[0].times.size(), std::size_t(2));
    QCOMPARE(projected[0].times[0].dayLabel, std::string("Monday"));
    QCOMPARE(projected[0].times[1].dayLabel, std::string("Friday"));
    QCOMPARE(projected[1].times.size(), std::size_t(2));
    QCOMPARE(projected[1].times[0].dayLabel, std::string("Tuesday"));
    QCOMPARE(projected[1].times[1].dayLabel, std::string("Thursday"));

    request.candidates[1].times[0] =
        time(0, 16 * 60 + 30, 17 * 60 + 30);
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
