#include "next/application/schedule_import_matching_projection.h"

#include <QtTest>

#include <cstdint>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next::Application;

namespace Domain = ClassMngr::Next::Domain;

static_assert(
    !std::is_assignable_v<Domain::TeacherId&, const Domain::ClassId&>
    );
static_assert(
    !std::is_assignable_v<Domain::ClassId&, const Domain::TeacherId&>
    );

namespace
{
Domain::TeacherId teacherIdentity(const std::int32_t value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

Domain::ClassId classIdentity(const std::int32_t value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

ScheduleImportMatchingCandidate candidate()
{
    ScheduleImportMatchingCandidate result;
    result.teacherKey = u"최선생";
    result.teacherName = u"최선생";
    result.rooms = {u"416"};
    result.roomMatchKeys = {u"416"};
    result.gradeMatchKey = u"e4";
    result.levelMatchKey = u"hercules";
    result.times = {{"Tuesday"}};
    return result;
}

ScheduleImportMatchingClass matchingClass(
    const std::int32_t id,
    const std::int32_t teacherId,
    const std::u16string roomMatchKey,
    std::vector<ScheduleImportMatchingTime> regularTimes = {},
    std::vector<ScheduleImportMatchingTime> intensiveTimes = {}
    )
{
    return {
        classIdentity(id),
        teacherIdentity(teacherId),
        id > 0,
        std::move(roomMatchKey),
        u"e4",
        u"hercules",
        std::move(regularTimes),
        std::move(intensiveTimes)
    };
}
}

class ScheduleImportMatchingProjectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void ranksEveryMatchCategoryAndKeepsStableTies();
    void reportsNoMatchAndInitiallyAbsentInventory();
    void excludesNonpositiveClassIdsFromMatchesAndSuggestions();
    void preservesNormalAndIntensiveFallbackSemantics();
    void preservesEmptyTeacherKeyMatchingSemantics();
};

void ScheduleImportMatchingProjectionTests::
ranksEveryMatchCategoryAndKeepsStableTies()
{
    ScheduleImportMatchingInput input;
    input.kind = ScheduleImportMatchingKind::Intensive;
    input.candidates = {candidate()};
    input.teachers = {
        {teacherIdentity(1), u"Dr. 최 선생 (room 416)"},
        {teacherIdentity(2), u"김선생"}
    };
    input.classes = {
        matchingClass(81, 1, u"416", {}, {{"Tuesday"}}),
        matchingClass(82, 1, u"416", {{"Tuesday"}}),
        matchingClass(92, 1, u"416", {}, {{"Thursday"}}),
        matchingClass(12, 1, u"416", {}, {{"Thursday"}}),
        matchingClass(94, 1, u"415", {}, {{"Tuesday"}}),
        matchingClass(95, 1, u"415", {}, {{"Thursday"}}),
        matchingClass(96, 2, u"416", {}, {{"Tuesday"}}),
        matchingClass(97, 2, u"417", {}, {{"Thursday"}})
    };

    const ScheduleImportMatchingProjection projection =
        projectScheduleImportMatching(input);

    QCOMPARE(projection.kind, ScheduleImportMatchingKind::Intensive);
    QCOMPARE(projection.teachers.size(), std::size_t{1});
    QVERIFY(projection.teachers[0].teacherKey == u"최선생");
    QVERIFY(
        projection.teachers[0].importedRooms
        == (std::vector<std::u16string>{u"416"})
        );
    QVERIFY(
        projection.teachers[0].matchingTeacherIds
        == (std::vector<Domain::TeacherId>{teacherIdentity(1)})
        );
    QCOMPARE(projection.teachers[0].affectedClassCount, std::size_t{6});
    QCOMPARE(projection.classes.size(), std::size_t{1});
    QVERIFY(
        projection.classes[0].matchingClassIds
        == (std::vector<Domain::ClassId>{
            classIdentity(81),
            classIdentity(82),
            classIdentity(92),
            classIdentity(12),
            classIdentity(94),
            classIdentity(95),
            classIdentity(96),
            classIdentity(97)
        })
        );
    QCOMPARE(projection.classes[0].candidateIndex, std::size_t{0});
    QVERIFY(projection.classes[0].suggestedClassId == classIdentity(81));
    QVERIFY(projection.classes[0].exactMatch);
    QCOMPARE(
        projection.classes[0].confidence,
        ScheduleImportMatchingConfidence::Confident
        );
    QCOMPARE(
        projection.classes[0].explanation,
        ScheduleImportMatchingExplanation::Exact
        );
    QCOMPARE(projection.inventory.classCount, std::size_t{8});
    QVERIFY(projection.inventory.hasRegularHours);
    QVERIFY(projection.inventory.hasIntensiveHours);
    QVERIFY(
        projection.initiallyAbsentClassIds
        == (std::vector<Domain::ClassId>{
            classIdentity(82),
            classIdentity(92),
            classIdentity(12),
            classIdentity(94),
            classIdentity(95),
            classIdentity(96),
            classIdentity(97)
        })
        );
}

void ScheduleImportMatchingProjectionTests::
reportsNoMatchAndInitiallyAbsentInventory()
{
    ScheduleImportMatchingInput input;
    input.candidates = {candidate()};
    input.classes = {
        matchingClass(7, 1, u"416", {{"Tuesday"}})
    };
    input.classes[0].gradeMatchKey = u"m1";

    const ScheduleImportMatchingProjection projection =
        projectScheduleImportMatching(input);

    QCOMPARE(projection.classes.size(), std::size_t{1});
    QVERIFY(projection.classes[0].matchingClassIds.empty());
    QVERIFY(!projection.classes[0].suggestedClassId.has_value());
    QVERIFY(!projection.classes[0].exactMatch);
    QCOMPARE(
        projection.classes[0].confidence,
        ScheduleImportMatchingConfidence::None
        );
    QCOMPARE(
        projection.classes[0].explanation,
        ScheduleImportMatchingExplanation::None
        );
    QCOMPARE(projection.inventory.classCount, std::size_t{1});
    QVERIFY(projection.inventory.hasRegularHours);
    QVERIFY(!projection.inventory.hasIntensiveHours);
    QVERIFY(
        projection.initiallyAbsentClassIds
        == (std::vector<Domain::ClassId>{classIdentity(7)})
        );
}

void ScheduleImportMatchingProjectionTests::
excludesNonpositiveClassIdsFromMatchesAndSuggestions()
{
    ScheduleImportMatchingInput input;
    input.candidates = {candidate()};
    input.teachers = {{teacherIdentity(1), u"최선생"}};
    input.classes = {
        matchingClass(0, 1, u"416", {{"Tuesday"}}),
        matchingClass(-7, 1, u"416", {{"Tuesday"}})
    };

    const ScheduleImportMatchingProjection projection =
        projectScheduleImportMatching(input);

    QCOMPARE(projection.classes.size(), std::size_t{1});
    QVERIFY(projection.classes[0].matchingClassIds.empty());
    QVERIFY(!projection.classes[0].suggestedClassId.has_value());
    QVERIFY(!projection.classes[0].exactMatch);
    QCOMPARE(
        projection.classes[0].confidence,
        ScheduleImportMatchingConfidence::None
        );
    QVERIFY(
        projection.initiallyAbsentClassIds
        == (std::vector<Domain::ClassId>{
            classIdentity(0),
            classIdentity(-7)
        })
        );
}

void ScheduleImportMatchingProjectionTests::
preservesNormalAndIntensiveFallbackSemantics()
{
    ScheduleImportMatchingInput input;
    input.candidates = {candidate()};
    input.teachers = {{teacherIdentity(1), u"최선생"}};
    input.classes = {
        matchingClass(43, 1, u"416", {{"Tuesday"}})
    };

    const ScheduleImportMatchingProjection normal =
        projectScheduleImportMatching(input);
    QVERIFY(
        normal.classes[0].matchingClassIds
        == (std::vector<Domain::ClassId>{classIdentity(43)})
        );
    QVERIFY(normal.classes[0].suggestedClassId == classIdentity(43));
    QVERIFY(normal.classes[0].exactMatch);
    QCOMPARE(
        normal.classes[0].confidence,
        ScheduleImportMatchingConfidence::Confident
        );

    input.kind = ScheduleImportMatchingKind::Intensive;
    const ScheduleImportMatchingProjection intensive =
        projectScheduleImportMatching(input);
    QVERIFY(
        intensive.classes[0].matchingClassIds
        == (std::vector<Domain::ClassId>{classIdentity(43)})
        );
    QVERIFY(intensive.classes[0].suggestedClassId == classIdentity(43));
    QVERIFY(!intensive.classes[0].exactMatch);
    QCOMPARE(
        intensive.classes[0].confidence,
        ScheduleImportMatchingConfidence::Possible
        );
    QCOMPARE(
        intensive.classes[0].explanation,
        ScheduleImportMatchingExplanation::PossibleWithOtherHours
        );
}

void ScheduleImportMatchingProjectionTests::
preservesEmptyTeacherKeyMatchingSemantics()
{
    ScheduleImportMatchingInput input;
    ScheduleImportMatchingCandidate imported = candidate();
    imported.teacherName = u"English";
    imported.teacherKey =
        ClassMngr::Next::Domain::KoreanTeacherKey::fromName(u"English").value();
    input.candidates = {imported};
    input.teachers = {
        {teacherIdentity(-1), u"English"},
        {teacherIdentity(0), u"English"}
    };
    input.classes = {
        matchingClass(14, -1, u"416", {{"Tuesday"}}),
        matchingClass(15, 0, u"416", {{"Tuesday"}})
    };

    const ScheduleImportMatchingProjection projection =
        projectScheduleImportMatching(input);

    QVERIFY(projection.teachers[0].teacherKey.empty());
    QVERIFY(
        projection.teachers[0].matchingTeacherIds
        == (std::vector<Domain::TeacherId>{
            teacherIdentity(-1),
            teacherIdentity(0)
        })
        );
    QCOMPARE(projection.teachers[0].affectedClassCount, std::size_t{2});
    QVERIFY(
        projection.classes[0].matchingClassIds
        == (std::vector<Domain::ClassId>{
            classIdentity(14),
            classIdentity(15)
        })
        );
    QVERIFY(projection.classes[0].suggestedClassId == classIdentity(14));
    QVERIFY(!projection.classes[0].exactMatch);
}

QTEST_GUILESS_MAIN(ScheduleImportMatchingProjectionTests)

#include "next_application_schedule_import_matching_projection_tests.moc"
