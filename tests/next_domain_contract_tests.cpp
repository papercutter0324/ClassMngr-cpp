#include "next/domain/course.h"
#include "next/domain/domain_types.h"
#include "next/domain/korean_teacher_key.h"
#include "next/domain/operation_result.h"
#include "next/domain/schedule_time.h"

#include <QtTest/QtTest>

#include <array>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Domain;

class NextDomainContractTests final : public QObject
{
    Q_OBJECT

private slots:
    void typedIdentifiersRejectEmptyValues();
    void typedIdentifiersRemainDistinct();
    void resultCarriesValueOrStructuredError();
    void scheduleTimesAcceptWeekdayAndMinuteBoundaries();
    void scheduleTimesRejectInvalidDaysAndIntervals();
    void scheduleTimesHaveValueAndOverlapSemantics();
    void coursesExposeOrderedSupportedPairs();
    void coursesHaveValueAndAccessorSemantics();
    void coursesRejectInvalidNamesAndCrossGradePairs();
    void coursesExposeWeeklyMeetingDayRules();
    void weeklyMeetingDayRulesRejectInvalidPatterns();
    void koreanTeacherKeysKeepEveryAcceptedRangeAndBoundary();
    void koreanTeacherKeysDiscardOtherCodeUnitsWithoutNormalization();
    void koreanTeacherKeysExposeEmptyAndValueSemantics();
};

void NextDomainContractTests::typedIdentifiersRejectEmptyValues()
{
    QVERIFY(!WorkspaceId::fromString("").has_value());

    const auto workspaceId = WorkspaceId::fromString("workspace-1");
    QVERIFY(workspaceId.has_value());
    QVERIFY(workspaceId->value() == "workspace-1");
}

void NextDomainContractTests::typedIdentifiersRemainDistinct()
{
    static_assert(!std::is_same_v<WorkspaceId, TeacherId>);
    static_assert(!std::is_same_v<ClassId, CampusId>);

    const auto first = ClassId::fromString("class-1");
    const auto second = ClassId::fromString("class-1");
    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    QVERIFY(*first == *second);
}

void NextDomainContractTests::resultCarriesValueOrStructuredError()
{
    const auto success = Result<int>::success(42);
    QVERIFY(success);
    QCOMPARE(success.value(), 42);

    const auto failure = Result<int>::failure(
        OperationError{
            .code = ErrorCode::Conflict,
            .message = "The import has conflicts.",
            .recoverable = true
        }
        );
    QVERIFY(!failure);
    QCOMPARE(failure.error().code, ErrorCode::Conflict);
    QVERIFY(failure.error().message == "The import has conflicts.");
    QVERIFY(failure.error().recoverable);

    const auto completed = Result<void>::success();
    QVERIFY(completed);
}

void NextDomainContractTests::scheduleTimesAcceptWeekdayAndMinuteBoundaries()
{
    for (int weekdayIndex = 0; weekdayIndex <= 6; ++weekdayIndex)
    {
        const auto value = ScheduleTime::fromMinutes(weekdayIndex, 0, 1);
        QVERIFY(value.has_value());
        QCOMPARE(value->weekdayIndex(), weekdayIndex);
    }

    const auto mondayAtStartOfDay = ScheduleTime::fromMinutes(
        static_cast<int>(Weekday::Monday),
        0,
        1
        );
    const auto sundayAtEndOfDay = ScheduleTime::fromMinutes(
        static_cast<int>(Weekday::Sunday),
        1438,
        1439
        );
    QVERIFY(mondayAtStartOfDay.has_value());
    QVERIFY(sundayAtEndOfDay.has_value());
    QVERIFY(mondayAtStartOfDay->weekday() == Weekday::Monday);
    QVERIFY(sundayAtEndOfDay->weekday() == Weekday::Sunday);
    QCOMPARE(sundayAtEndOfDay->startMinute(), 1438);
    QCOMPARE(sundayAtEndOfDay->endMinute(), 1439);
}

void NextDomainContractTests::scheduleTimesRejectInvalidDaysAndIntervals()
{
    QVERIFY(!ScheduleTime::fromMinutes(-1, 0, 1).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(7, 0, 1).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, -1, 1).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, 1440, 1441).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, 0, -1).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, 12, 12).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, 13, 12).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(6, 1439, 1440).has_value());
}

void NextDomainContractTests::scheduleTimesHaveValueAndOverlapSemantics()
{
    const auto first = ScheduleTime::fromMinutes(0, 0, 60);
    const auto sameValue = ScheduleTime::fromMinutes(0, 0, 60);
    const auto adjacent = ScheduleTime::fromMinutes(0, 60, 120);
    const auto overlapping = ScheduleTime::fromMinutes(0, 59, 120);
    const auto differentWeekday = ScheduleTime::fromMinutes(1, 0, 60);
    QVERIFY(first.has_value());
    QVERIFY(sameValue.has_value());
    QVERIFY(adjacent.has_value());
    QVERIFY(overlapping.has_value());
    QVERIFY(differentWeekday.has_value());

    const ScheduleTime copy = *first;
    QVERIFY(copy == *first);
    QVERIFY(copy == *sameValue);
    QVERIFY(copy != *adjacent);
    QVERIFY(!first->overlaps(*adjacent));
    QVERIFY(first->overlaps(*overlapping));
    QVERIFY(!first->overlaps(*differentWeekday));
}

void NextDomainContractTests::coursesExposeOrderedSupportedPairs()
{
    using CourseNames = std::pair<std::string_view, std::string_view>;
    const std::array<CourseNames, 25> supportedPairs{{
        {"E4", "Theseus"},
        {"E4", "Perseus"},
        {"E4", "Odysseus"},
        {"E4", "Hercules"},
        {"E5", "Artemis"},
        {"E5", "Hermes"},
        {"E5", "Apollo"},
        {"E5", "Zeus"},
        {"E5", "Athena"},
        {"E6", "Helios"},
        {"E6", "Poseidon"},
        {"E6", "Gaia"},
        {"E6", "Hera"},
        {"E6", "Song's"},
        {"M1", "Elephantus"},
        {"M1", "Galaxia"},
        {"M1", "Solis"},
        {"M1", "Major"},
        {"M1", "Song's"},
        {"M2", "Ursa"},
        {"M2", "Leo"},
        {"M2", "Tigris"},
        {"M2", "Major"},
        {"M2", "Song's"},
        {"M3", "Song's"}
    }};
    const std::array<std::string_view, 6> expectedGrades{
        "E4", "E5", "E6", "M1", "M2", "M3"
    };
    const std::array<std::vector<std::string_view>, 6> expectedLevels{{
        {"Theseus", "Perseus", "Odysseus", "Hercules"},
        {"Artemis", "Hermes", "Apollo", "Zeus", "Athena"},
        {"Helios", "Poseidon", "Gaia", "Hera", "Song's"},
        {"Elephantus", "Galaxia", "Solis", "Major", "Song's"},
        {"Ursa", "Leo", "Tigris", "Major", "Song's"},
        {"Song's"}
    }};

    const auto grades = Course::grades();
    QCOMPARE(static_cast<int>(grades.size()),
        static_cast<int>(expectedGrades.size()));
    for (std::size_t index = 0; index < expectedGrades.size(); ++index)
    {
        QVERIFY(grades[index] == expectedGrades[index]);
        QVERIFY(
            Course::levelsForGrade(expectedGrades[index])
            == expectedLevels[index]
            );
    }

    for (const CourseNames& names : supportedPairs)
    {
        const auto course = Course::fromNames(names.first, names.second);
        QVERIFY(course.has_value());
        QVERIFY(course->grade() == names.first);
        QVERIFY(course->level() == names.second);
    }
}

void NextDomainContractTests::coursesHaveValueAndAccessorSemantics()
{
    const auto first = Course::fromNames("E4", "Theseus");
    const auto sameValue = Course::fromNames("E4", "Theseus");
    const auto otherLevel = Course::fromNames("E4", "Perseus");
    QVERIFY(first.has_value());
    QVERIFY(sameValue.has_value());
    QVERIFY(otherLevel.has_value());
    QVERIFY(*first == *sameValue);
    QVERIFY(*first != *otherLevel);
    QVERIFY(first->grade() == "E4");
    QVERIFY(first->level() == "Theseus");
}

void NextDomainContractTests::coursesRejectInvalidNamesAndCrossGradePairs()
{
    QVERIFY(!Course::fromNames("", "Theseus").has_value());
    QVERIFY(!Course::fromNames("E7", "Theseus").has_value());
    QVERIFY(!Course::fromNames("E4", "Unknown").has_value());
    QVERIFY(!Course::fromNames("E4", "Zeus").has_value());
    QVERIFY(!Course::fromNames("M3", "Zeus").has_value());
    QVERIFY(!Course::fromNames("e4", "Theseus").has_value());
    QVERIFY(!Course::fromNames("E4", "theseus").has_value());
    QVERIFY(Course::levelsForGrade("E7").empty());
}

void NextDomainContractTests::coursesExposeWeeklyMeetingDayRules()
{
    const auto theseus = Course::fromNames("E4", "Theseus");
    const auto athena = Course::fromNames("E5", "Athena");
    const auto standardE5 = Course::fromNames("E5", "Zeus");
    const auto standardE6 = Course::fromNames("E6", "Hera");
    const auto songsE6 = Course::fromNames("E6", "Song's");
    const auto standardM1 = Course::fromNames("M1", "Elephantus");
    const auto songsM1 = Course::fromNames("M1", "Song's");
    const auto songsM3 = Course::fromNames("M3", "Song's");
    QVERIFY(theseus.has_value());
    QVERIFY(athena.has_value());
    QVERIFY(standardE5.has_value());
    QVERIFY(standardE6.has_value());
    QVERIFY(songsE6.has_value());
    QVERIFY(standardM1.has_value());
    QVERIFY(songsM1.has_value());
    QVERIFY(songsM3.has_value());

    const auto pairedRule = theseus->weeklyMeetingDayRule();
    QVERIFY(pairedRule.has_value());
    QVERIFY(
        pairedRule->kind()
        == Course::WeeklyMeetingDayRuleKind::PairedWeekdays
        );
    QVERIFY(pairedRule->allowedPatterns().size() == 4);
    QVERIFY(pairedRule->allows({Weekday::Monday, Weekday::Wednesday}));
    QVERIFY(pairedRule->allows({Weekday::Tuesday, Weekday::Thursday}));
    QVERIFY(!pairedRule->allows({Weekday::Monday}));

    const auto athenaRule = athena->weeklyMeetingDayRule();
    QVERIFY(athenaRule.has_value());
    QVERIFY(
        athenaRule->kind()
        == Course::WeeklyMeetingDayRuleKind::ThreeDayOrTuesdayThursday
        );
    QVERIFY(athenaRule->allows(
        {Weekday::Monday, Weekday::Wednesday, Weekday::Friday}
        ));
    QVERIFY(athenaRule->allows({Weekday::Tuesday, Weekday::Thursday}));
    QVERIFY(!athenaRule->allows({Weekday::Monday, Weekday::Wednesday}));

    const auto standardE5Rule = standardE5->weeklyMeetingDayRule();
    QVERIFY(standardE5Rule.has_value());
    QVERIFY(
        standardE5Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::PairedWeekdays
        );
    QVERIFY(standardE5Rule->allows({Weekday::Monday, Weekday::Friday}));
    QVERIFY(!standardE5Rule->allows(
        {Weekday::Monday, Weekday::Wednesday, Weekday::Friday}
        ));

    const auto standardE6Rule = standardE6->weeklyMeetingDayRule();
    QVERIFY(standardE6Rule.has_value());
    QVERIFY(
        standardE6Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::SingleWeekday
        );
    QVERIFY(standardE6Rule->allows({Weekday::Friday}));
    QVERIFY(!standardE6Rule->allows({Weekday::Monday, Weekday::Wednesday}));

    const auto songsE6Rule = songsE6->weeklyMeetingDayRule();
    QVERIFY(songsE6Rule.has_value());
    QVERIFY(
        songsE6Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::ThreeDayOrTuesdayThursday
        );
    QVERIFY(songsE6Rule->allows(
        {Weekday::Monday, Weekday::Wednesday, Weekday::Friday}
        ));

    const auto standardM1Rule = standardM1->weeklyMeetingDayRule();
    QVERIFY(standardM1Rule.has_value());
    QVERIFY(
        standardM1Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::SingleWeekday
        );
    QVERIFY(standardM1Rule->allows({Weekday::Tuesday}));
    QVERIFY(!standardM1Rule->allows({Weekday::Monday, Weekday::Friday}));

    const auto songsM1Rule = songsM1->weeklyMeetingDayRule();
    QVERIFY(songsM1Rule.has_value());
    QVERIFY(
        songsM1Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::PairedWeekdays
        );
    QVERIFY(songsM1Rule->allows({Weekday::Wednesday, Weekday::Friday}));

    const auto songsM3Rule = songsM3->weeklyMeetingDayRule();
    QVERIFY(songsM3Rule.has_value());
    QVERIFY(
        songsM3Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::PairedWeekdays
        );
    QVERIFY(songsM3Rule->allows({Weekday::Monday, Weekday::Friday}));

    QVERIFY(!Course::weeklyMeetingDayRuleFor(
        CourseGradeBand::M3,
        CourseLevelCategory::Standard
        ).has_value());
}

void NextDomainContractTests::weeklyMeetingDayRulesRejectInvalidPatterns()
{
    const auto rule = Course::weeklyMeetingDayRuleFor(
        CourseGradeBand::E4,
        CourseLevelCategory::Standard
        );
    QVERIFY(rule.has_value());

    QVERIFY(rule->allows({Weekday::Wednesday, Weekday::Monday}));
    QVERIFY(!rule->allows({Weekday::Monday, Weekday::Monday}));
    QVERIFY(!rule->allows({Weekday::Monday, Weekday::Saturday}));
    QVERIFY(!rule->allows({Weekday::Sunday}));
    QVERIFY(!rule->allows({static_cast<Weekday>(7)}));
    QVERIFY(!rule->allows({}));
}

void NextDomainContractTests::
koreanTeacherKeysKeepEveryAcceptedRangeAndBoundary()
{
    const std::u16string accepted{
        u'\u1100', u'\u1101', u'\u11ff',
        u'\u3130', u'\u3131', u'\u318f',
        u'\ua960', u'\ua961', u'\ua97f',
        u'\uac00', u'\uac01', u'\ud7af',
        u'\ud7b0', u'\ud7b1', u'\ud7ff'
    };
    const KoreanTeacherKey key = KoreanTeacherKey::fromName(accepted);
    QVERIFY(key.value() == accepted);

    for (const char16_t codeUnit : accepted)
    {
        QVERIFY(KoreanTeacherKey::isHangulCodeUnit(codeUnit));
    }

    const std::u16string rejected{
        u'\u10ff', u'\u1200',
        u'\u312f', u'\u3190',
        u'\ua95f', u'\ua980',
        u'\uabff', static_cast<char16_t>(0xd800), u'\ue000'
    };
    const KoreanTeacherKey empty = KoreanTeacherKey::fromName(rejected);
    QVERIFY(empty.empty());
    QVERIFY(empty.value().empty());
    for (const char16_t codeUnit : rejected)
    {
        QVERIFY(!KoreanTeacherKey::isHangulCodeUnit(codeUnit));
    }
}

void NextDomainContractTests::
koreanTeacherKeysDiscardOtherCodeUnitsWithoutNormalization()
{
    const KoreanTeacherKey mixed = KoreanTeacherKey::fromName(
        u"A\u1100-\u3131 \uac00\ud7ffZ"
        );
    QVERIFY(mixed.value() == u"\u1100\u3131\uac00\ud7ff");

    const KoreanTeacherKey composed = KoreanTeacherKey::fromName(u"\ud55c");
    const KoreanTeacherKey decomposed =
        KoreanTeacherKey::fromName(u"\u1112\u1161\u11ab");
    QVERIFY(composed.value() == u"\ud55c");
    QVERIFY(decomposed.value() == u"\u1112\u1161\u11ab");
    QVERIFY(composed != decomposed);

    const KoreanTeacherKey compatibility =
        KoreanTeacherKey::fromName(u"\u3131");
    const KoreanTeacherKey leadingJamo =
        KoreanTeacherKey::fromName(u"\u1100");
    QVERIFY(compatibility.value() == u"\u3131");
    QVERIFY(leadingJamo.value() == u"\u1100");
    QVERIFY(compatibility != leadingJamo);
}

void NextDomainContractTests::koreanTeacherKeysExposeEmptyAndValueSemantics()
{
    const KoreanTeacherKey empty = KoreanTeacherKey::fromName(u"A 1");
    QVERIFY(empty.empty());
    QVERIFY(empty.value().empty());

    const KoreanTeacherKey first = KoreanTeacherKey::fromName(u"A\uac00");
    const KoreanTeacherKey sameValue = KoreanTeacherKey::fromName(u"\uac00");
    const KoreanTeacherKey different = KoreanTeacherKey::fromName(u"\uac01");
    QVERIFY(!first.empty());
    QVERIFY(first.value() == u"\uac00");
    QVERIFY(first == sameValue);
    QVERIFY(first != different);
}

QTEST_APPLESS_MAIN(NextDomainContractTests)

#include "next_domain_contract_tests.moc"
