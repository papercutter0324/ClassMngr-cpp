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
static_assert(
    std::is_same_v<
        decltype(ScheduleImportMatchingCandidate::teacherKey),
        Domain::KoreanTeacherKey
        >
    );
static_assert(
    std::is_same_v<
        decltype(ScheduleImportMatchingTeacherProjection::teacherKey),
        Domain::KoreanTeacherKey
        >
    );
static_assert(
    std::is_same_v<
        decltype(ScheduleImportMatchingTeacherProjection::affectedClassCount),
        ScheduleImportMatchingClassCount
        >
    );
static_assert(
    std::is_same_v<
        decltype(ScheduleImportMatchingInventory::classCount),
        ScheduleImportMatchingClassCount
        >
    );
static_assert(
    std::is_same_v<
        decltype(ScheduleImportMatchingClassProjection::candidateIndex),
        ScheduleImportMatchingCandidateIndex
        >
    );
static_assert(
    std::is_same_v<
        decltype(ScheduleImportMatchingClassCount{}.rawValue()),
        std::size_t
        >
    );
static_assert(
    std::is_same_v<
        decltype(ScheduleImportMatchingCandidateIndex{}.rawValue()),
        std::size_t
        >
    );
static_assert(
    std::is_constructible_v<ScheduleImportMatchingCandidateIndex, std::size_t>
    );
static_assert(
    !std::is_convertible_v<std::size_t, ScheduleImportMatchingCandidateIndex>
    );
static_assert(
    !std::is_convertible_v<ScheduleImportMatchingCandidateIndex, std::size_t>
    );
static_assert(
    std::is_constructible_v<ScheduleImportMatchingClassCount, std::size_t>
    );
static_assert(
    !std::is_convertible_v<std::size_t, ScheduleImportMatchingClassCount>
    );
static_assert(
    !std::is_convertible_v<ScheduleImportMatchingClassCount, std::size_t>
    );
static_assert(
    std::is_same_v<
        decltype(projectScheduleImportMatching(ScheduleImportMatchingInput{})),
        std::expected<
            ScheduleImportMatchingProjection,
            ScheduleImportMatchingFailure
            >
        >
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
    result.teacherKey = Domain::KoreanTeacherKey::fromName(u"최선생");
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
    void matchingClassCountHasTypedZeroAndPositiveValues();
    void matchingCandidateIndexHasTypedZeroAndPositiveValues();
    void rejectsUnknownKindWithoutReturningProjection();
    void ranksEveryMatchCategoryAndKeepsStableTies();
    void projectsCandidateIndexesInInputOrder();
    void reportsNoMatchAndInitiallyAbsentInventory();
    void excludesNonpositiveClassIdsFromMatchesAndSuggestions();
    void preservesNormalAndIntensiveFallbackSemantics();
    void preservesEmptyTeacherKeyMatchingSemantics();
};

void ScheduleImportMatchingProjectionTests::
matchingClassCountHasTypedZeroAndPositiveValues()
{
    const ScheduleImportMatchingClassCount zero;
    const ScheduleImportMatchingClassCount positive{6};

    QCOMPARE(zero.rawValue(), std::size_t{0});
    QCOMPARE(positive.rawValue(), std::size_t{6});
    QVERIFY(zero == ScheduleImportMatchingClassCount{});
    QVERIFY(positive == ScheduleImportMatchingClassCount{6});
}

void ScheduleImportMatchingProjectionTests::
matchingCandidateIndexHasTypedZeroAndPositiveValues()
{
    const ScheduleImportMatchingCandidateIndex zero;
    const ScheduleImportMatchingCandidateIndex positive{6};

    QCOMPARE(zero.rawValue(), std::size_t{0});
    QCOMPARE(positive.rawValue(), std::size_t{6});
    QVERIFY(zero == ScheduleImportMatchingCandidateIndex{});
    QVERIFY(positive == ScheduleImportMatchingCandidateIndex{6});
}

void ScheduleImportMatchingProjectionTests::
rejectsUnknownKindWithoutReturningProjection()
{
    for (const int kind : {-1, 255})
    {
        ScheduleImportMatchingInput input;
        input.kind = static_cast<ScheduleImportMatchingKind>(kind);
        const auto empty = projectScheduleImportMatching(input);
        QVERIFY(!empty.has_value());
        QCOMPARE(
            empty.error().code,
            ScheduleImportMatchingErrorCode::InvalidKind
            );

        input.candidates = {candidate()};
        input.teachers = {{teacherIdentity(1), candidate().teacherName}};
        input.classes = {
            matchingClass(43, 1, u"416", {{"Tuesday"}})
        };
        const auto populated = projectScheduleImportMatching(input);
        QVERIFY(!populated.has_value());
        QCOMPARE(
            populated.error().code,
            ScheduleImportMatchingErrorCode::InvalidKind
            );
    }
}

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

    const auto outcome = projectScheduleImportMatching(input);
    QVERIFY(outcome.has_value());
    const ScheduleImportMatchingProjection& projection = *outcome;

    QCOMPARE(projection.kind, ScheduleImportMatchingKind::Intensive);
    QCOMPARE(projection.teachers.size(), std::size_t{1});
    QVERIFY(
        projection.teachers[0].teacherKey
        == Domain::KoreanTeacherKey::fromName(u"최선생")
        );
    QVERIFY(
        projection.teachers[0].importedRooms
        == (std::vector<std::u16string>{u"416"})
        );
    QVERIFY(
        projection.teachers[0].matchingTeacherIds
        == (std::vector<Domain::TeacherId>{teacherIdentity(1)})
        );
    QVERIFY(
        projection.teachers[0].affectedClassCount
        == ScheduleImportMatchingClassCount{6}
        );
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
    QVERIFY(
        projection.classes[0].candidateIndex
        == ScheduleImportMatchingCandidateIndex{0}
        );
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
    QVERIFY(
        projection.inventory.classCount
        == ScheduleImportMatchingClassCount{8}
        );
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
projectsCandidateIndexesInInputOrder()
{
    ScheduleImportMatchingInput input;
    input.candidates = {candidate(), candidate()};
    input.candidates[1].teacherKey =
        Domain::KoreanTeacherKey::fromName(u"김선생");
    input.candidates[1].teacherName = u"김선생";
    input.candidates[1].rooms = {u"417"};
    input.candidates[1].roomMatchKeys = {u"417"};

    const auto outcome = projectScheduleImportMatching(input);
    QVERIFY(outcome.has_value());
    const ScheduleImportMatchingProjection& projection = *outcome;

    QCOMPARE(projection.classes.size(), std::size_t{2});
    QVERIFY(
        projection.classes[0].candidateIndex
        == ScheduleImportMatchingCandidateIndex{0}
        );
    QVERIFY(
        projection.classes[1].candidateIndex
        == ScheduleImportMatchingCandidateIndex{1}
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

    const auto outcome = projectScheduleImportMatching(input);
    QVERIFY(outcome.has_value());
    const ScheduleImportMatchingProjection& projection = *outcome;

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
    QVERIFY(
        projection.inventory.classCount
        == ScheduleImportMatchingClassCount{1}
        );
    QVERIFY(
        projection.teachers[0].affectedClassCount
        == ScheduleImportMatchingClassCount{}
        );
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

    const auto outcome = projectScheduleImportMatching(input);
    QVERIFY(outcome.has_value());
    const ScheduleImportMatchingProjection& projection = *outcome;

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

    const auto normalOutcome = projectScheduleImportMatching(input);
    QVERIFY(normalOutcome.has_value());
    const ScheduleImportMatchingProjection& normal = *normalOutcome;
    QCOMPARE(normal.kind, ScheduleImportMatchingKind::Normal);
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
    const auto intensiveOutcome = projectScheduleImportMatching(input);
    QVERIFY(intensiveOutcome.has_value());
    const ScheduleImportMatchingProjection& intensive = *intensiveOutcome;
    QCOMPARE(intensive.kind, ScheduleImportMatchingKind::Intensive);
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

    input.classes = {
        matchingClass(43, 1, u"416", {}, {{"Tuesday"}})
    };
    input.kind = ScheduleImportMatchingKind::Normal;
    const auto normalFallback = projectScheduleImportMatching(input);
    QVERIFY(normalFallback.has_value());
    QVERIFY(normalFallback->classes[0].suggestedClassId == classIdentity(43));
    QVERIFY(!normalFallback->classes[0].exactMatch);
    QCOMPARE(
        normalFallback->classes[0].confidence,
        ScheduleImportMatchingConfidence::Possible
        );
    QCOMPARE(
        normalFallback->classes[0].explanation,
        ScheduleImportMatchingExplanation::PossibleWithOtherHours
        );

    input.kind = ScheduleImportMatchingKind::Intensive;
    const auto intensiveTarget = projectScheduleImportMatching(input);
    QVERIFY(intensiveTarget.has_value());
    QVERIFY(intensiveTarget->classes[0].suggestedClassId == classIdentity(43));
    QVERIFY(intensiveTarget->classes[0].exactMatch);
    QCOMPARE(
        intensiveTarget->classes[0].confidence,
        ScheduleImportMatchingConfidence::Confident
        );
}

void ScheduleImportMatchingProjectionTests::
preservesEmptyTeacherKeyMatchingSemantics()
{
    ScheduleImportMatchingInput input;
    ScheduleImportMatchingCandidate imported = candidate();
    imported.teacherName = u"English";
    imported.teacherKey = Domain::KoreanTeacherKey::fromName(u"English");
    input.candidates = {imported};
    input.teachers = {
        {teacherIdentity(-1), u"English"},
        {teacherIdentity(0), u"English"}
    };
    input.classes = {
        matchingClass(14, -1, u"416", {{"Tuesday"}}),
        matchingClass(15, 0, u"416", {{"Tuesday"}})
    };

    const auto outcome = projectScheduleImportMatching(input);
    QVERIFY(outcome.has_value());
    const ScheduleImportMatchingProjection& projection = *outcome;

    QVERIFY(
        projection.teachers[0].teacherKey
        == Domain::KoreanTeacherKey::fromName(u"")
        );
    QVERIFY(projection.teachers[0].teacherKey.empty());
    QVERIFY(
        projection.teachers[0].matchingTeacherIds
        == (std::vector<Domain::TeacherId>{
            teacherIdentity(-1),
            teacherIdentity(0)
        })
        );
    QVERIFY(
        projection.teachers[0].affectedClassCount
        == ScheduleImportMatchingClassCount{2}
        );
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
