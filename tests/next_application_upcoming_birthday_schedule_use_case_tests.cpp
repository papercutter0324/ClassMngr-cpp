#include "next/application/upcoming_birthday_schedule_use_case.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;
using ClassMngr::Next::Domain::GregorianDate;

namespace
{

GregorianDate gregorianDate(const int year, const int month, const int day)
{
    return GregorianDate::fromComponents(year, month, day).value();
}

int compareUtf16(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return left.compare(right);
}

UpcomingBirthdaySchedule buildSchedule(
    const std::vector<UpcomingBirthdayCandidate>& candidates,
    const UpcomingBirthdayDate& referenceDate
    )
{
    return UpcomingBirthdayScheduleUseCase::build(
        candidates,
        referenceDate,
        compareUtf16
        );
}

UpcomingBirthdayCandidate birthday(
    std::string monthDay,
    std::u16string displayName,
    const UpcomingBirthdayStaffGroup group =
        UpcomingBirthdayStaffGroup::KoreanTeacher,
    std::u16string position = {}
    )
{
    return {
        .birthdayMonthDay = UpcomingBirthdayMonthDayInput(std::move(monthDay)),
        .displayName = std::move(displayName),
        .position = std::move(position),
        .group = group
    };
}

}

class NextApplicationUpcomingBirthdayScheduleUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void bucketsTodayThisWeekAndNextWeekThroughSunday();
    void selectsOccurrencesAcrossCalendarYearRollover();
    void mapsLeapDayToFebruaryTwentyEighthInNonLeapYear();
    void acceptsOpaqueBirthdayBytesAndSkipsThem();
    void omitsInvalidBirthdaysAndBlankNames();
    void trimsOnlyAsciiWhitespaceAtBirthdayEdges();
    void omitsCandidatesWithUnknownStaffGroups();
    void preservesEveryDeclaredStaffGroup();
    void preservesNonAsciiUtf16DisplayName();
    void sortsEveryBucketByDateNameGroupAndPosition();
    void capsRangesAtMaximumDate();
    void invalidReferenceDateReturnsEmptySchedule();
};

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
bucketsTodayThisWeekAndNextWeekThroughSunday()
{
    const UpcomingBirthdaySchedule schedule =
        buildSchedule(
            {
                birthday(" 08-18 ", u"Today"),
                birthday("08-23", u"This week Sunday"),
                birthday("08-24", u"Next week Monday"),
                birthday("08-30", u"Next week Sunday"),
                birthday("08-31", u"After range")
            },
            {2026, 8, 18}
            );

    QCOMPARE(schedule.today.size(), std::size_t{1});
    QVERIFY((schedule.today.front().date == gregorianDate(2026, 8, 18)));
    QCOMPARE(schedule.today.front().displayName, std::u16string(u"Today"));

    QCOMPARE(schedule.thisWeek.size(), std::size_t{1});
    QVERIFY((schedule.thisWeek.front().date == gregorianDate(2026, 8, 23)));
    QCOMPARE(
        schedule.thisWeek.front().displayName,
        std::u16string(u"This week Sunday")
        );

    QCOMPARE(schedule.nextWeek.size(), std::size_t{2});
    QVERIFY((schedule.nextWeek[0].date == gregorianDate(2026, 8, 24)));
    QVERIFY((schedule.nextWeek[1].date == gregorianDate(2026, 8, 30)));
    QVERIFY(schedule.nextWeek[1].group == UpcomingBirthdayStaffGroup::KoreanTeacher);
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
selectsOccurrencesAcrossCalendarYearRollover()
{
    const UpcomingBirthdaySchedule schedule =
        buildSchedule(
            {
                birthday("01-03", u"This week"),
                birthday("01-04", u"Next week"),
                birthday("01-10", u"Next week Sunday"),
                birthday("01-11", u"After range")
            },
            {2026, 12, 28}
            );

    QCOMPARE(schedule.thisWeek.size(), std::size_t{1});
    QVERIFY((schedule.thisWeek.front().date == gregorianDate(2027, 1, 3)));
    QCOMPARE(schedule.nextWeek.size(), std::size_t{2});
    QVERIFY((schedule.nextWeek[0].date == gregorianDate(2027, 1, 4)));
    QVERIFY((schedule.nextWeek[1].date == gregorianDate(2027, 1, 10)));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
mapsLeapDayToFebruaryTwentyEighthInNonLeapYear()
{
    const UpcomingBirthdaySchedule schedule =
        buildSchedule(
            {birthday("02-29", u"Leap Day")},
            {2027, 2, 22}
            );

    QCOMPARE(schedule.thisWeek.size(), std::size_t{1});
    QVERIFY((schedule.thisWeek.front().date == gregorianDate(2027, 2, 28)));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
acceptsOpaqueBirthdayBytesAndSkipsThem()
{
    const std::string malformedUtf8("\xC3\x28", 2);
    const UpcomingBirthdayCandidate blankBirthday{
        .birthdayMonthDay = UpcomingBirthdayMonthDayInput(std::string{}),
        .displayName = u"Blank birthday"
    };
    const UpcomingBirthdayCandidate malformedBirthday{
        .birthdayMonthDay = UpcomingBirthdayMonthDayInput(malformedUtf8),
        .displayName = u"Malformed birthday"
    };

    QCOMPARE(blankBirthday.birthdayMonthDay.value(), std::string{});
    QCOMPARE(malformedBirthday.birthdayMonthDay.value(), malformedUtf8);

    const UpcomingBirthdaySchedule schedule = buildSchedule(
        {
            blankBirthday,
            malformedBirthday,
            birthday("08-18", u"Valid birthday")
        },
        {2026, 8, 18}
        );

    QCOMPARE(schedule.today.size(), std::size_t{1});
    QCOMPARE(schedule.today.front().displayName, std::u16string(u"Valid birthday"));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
omitsInvalidBirthdaysAndBlankNames()
{
    const UpcomingBirthdaySchedule schedule =
        buildSchedule(
            {
                birthday("02-30", u"Invalid day"),
                birthday("13-01", u"Invalid month"),
                birthday("2-03", u"Non-padded month"),
                birthday("08-18", u""),
                birthday("08-18", u"\u00A0 \t"),
                birthday("08-18", u"\u0085"),
                birthday("08-18", u"Visible")
            },
            {2026, 8, 18}
            );

    QCOMPARE(schedule.today.size(), std::size_t{1});
    QCOMPARE(schedule.today.front().displayName, std::u16string(u"Visible"));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
trimsOnlyAsciiWhitespaceAtBirthdayEdges()
{
    const UpcomingBirthdaySchedule schedule =
        buildSchedule(
            {
                birthday("\t\n\v" "08-18" "\f\r ", u"ASCII whitespace"),
                birthday("\xC2\xA0" "08-18", u"Unicode prefix space"),
                birthday("08-18" "\xC2\xA0", u"Unicode suffix space")
            },
            {2026, 8, 18}
            );

    QCOMPARE(schedule.today.size(), std::size_t{1});
    QCOMPARE(schedule.today.front().displayName, std::u16string(u"ASCII whitespace"));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
omitsCandidatesWithUnknownStaffGroups()
{
    const UpcomingBirthdaySchedule schedule =
        buildSchedule(
            {
                birthday(
                    "08-18",
                    u"Unknown group",
                    static_cast<UpcomingBirthdayStaffGroup>(99)
                    ),
                birthday("08-18", u"Known group")
            },
            {2026, 8, 18}
            );

    QCOMPARE(schedule.today.size(), std::size_t{1});
    QCOMPARE(schedule.today.front().displayName, std::u16string(u"Known group"));
    QVERIFY(schedule.thisWeek.empty());
    QVERIFY(schedule.nextWeek.empty());
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
preservesEveryDeclaredStaffGroup()
{
    const UpcomingBirthdaySchedule schedule =
        buildSchedule(
            {
                birthday(
                    "08-18",
                    u"Same name",
                    UpcomingBirthdayStaffGroup::KoreanTeacher
                    ),
                birthday(
                    "08-18",
                    u"Same name",
                    UpcomingBirthdayStaffGroup::NativeEnglishTeacher
                    ),
                birthday("08-18", u"Same name", UpcomingBirthdayStaffGroup::GsTeam)
            },
            {2026, 8, 18}
            );

    QCOMPARE(schedule.today.size(), std::size_t{3});
    QVERIFY(schedule.today[0].group == UpcomingBirthdayStaffGroup::KoreanTeacher);
    QVERIFY(schedule.today[1].group == UpcomingBirthdayStaffGroup::NativeEnglishTeacher);
    QVERIFY(schedule.today[2].group == UpcomingBirthdayStaffGroup::GsTeam);
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
preservesNonAsciiUtf16DisplayName()
{
    const UpcomingBirthdaySchedule schedule =
        buildSchedule(
            {birthday("08-18", u"\uAE40\uBBFC\uC9C0")},
            {2026, 8, 18}
            );

    QCOMPARE(schedule.today.size(), std::size_t{1});
    QCOMPARE(
        schedule.today.front().displayName,
        std::u16string(u"\uAE40\uBBFC\uC9C0")
        );
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
sortsEveryBucketByDateNameGroupAndPosition()
{
    const UpcomingBirthdaySchedule schedule = buildSchedule(
        {
            birthday("08-18", u"B today"),
            birthday("08-18", u"A today"),
            birthday("08-19", u"Zulu", UpcomingBirthdayStaffGroup::KoreanTeacher),
            birthday("08-19", u"Same", UpcomingBirthdayStaffGroup::GsTeam, u"Team"),
            birthday("08-19", u"Same", UpcomingBirthdayStaffGroup::NativeEnglishTeacher, u"Z"),
            birthday("08-19", u"Same", UpcomingBirthdayStaffGroup::NativeEnglishTeacher, u"A"),
            birthday("08-19", u"Same", UpcomingBirthdayStaffGroup::KoreanTeacher, u"Z"),
            birthday("08-20", u"A later date"),
            birthday("08-24", u"B next week"),
            birthday("08-24", u"A next week")
        },
        {2026, 8, 18}
        );

    QCOMPARE(schedule.today.size(), std::size_t{2});
    QCOMPARE(schedule.today[0].displayName, std::u16string(u"A today"));
    QCOMPARE(schedule.today[1].displayName, std::u16string(u"B today"));

    QCOMPARE(schedule.thisWeek.size(), std::size_t{6});
    QCOMPARE(schedule.thisWeek[0].displayName, std::u16string(u"Same"));
    QVERIFY(schedule.thisWeek[0].group == UpcomingBirthdayStaffGroup::KoreanTeacher);
    QCOMPARE(schedule.thisWeek[0].position, std::u16string(u"Z"));
    QCOMPARE(schedule.thisWeek[1].displayName, std::u16string(u"Same"));
    QVERIFY(schedule.thisWeek[1].group == UpcomingBirthdayStaffGroup::NativeEnglishTeacher);
    QCOMPARE(schedule.thisWeek[1].position, std::u16string(u"A"));
    QCOMPARE(schedule.thisWeek[2].position, std::u16string(u"Z"));
    QVERIFY(schedule.thisWeek[3].group == UpcomingBirthdayStaffGroup::GsTeam);
    QCOMPARE(schedule.thisWeek[4].displayName, std::u16string(u"Zulu"));
    QVERIFY((schedule.thisWeek[4].date == gregorianDate(2026, 8, 19)));
    QCOMPARE(schedule.thisWeek[5].displayName, std::u16string(u"A later date"));
    QVERIFY((schedule.thisWeek[5].date == gregorianDate(2026, 8, 20)));

    QCOMPARE(schedule.nextWeek.size(), std::size_t{2});
    QCOMPARE(schedule.nextWeek[0].displayName, std::u16string(u"A next week"));
    QCOMPARE(schedule.nextWeek[1].displayName, std::u16string(u"B next week"));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
capsRangesAtMaximumDate()
{
    const UpcomingBirthdaySchedule partialNextWeek =
        buildSchedule(
            {
                birthday("12-20", u"Today"),
                birthday("12-26", u"This week"),
                birthday("12-31", u"Partial next week"),
                birthday("01-01", u"Beyond maximum")
            },
            {9999, 12, 20}
            );

    QCOMPARE(partialNextWeek.today.size(), std::size_t{1});
    QVERIFY((partialNextWeek.today.front().date == gregorianDate(9999, 12, 20)));
    QCOMPARE(partialNextWeek.thisWeek.size(), std::size_t{1});
    QVERIFY((partialNextWeek.thisWeek.front().date == gregorianDate(9999, 12, 26)));
    QCOMPARE(partialNextWeek.nextWeek.size(), std::size_t{1});
    QVERIFY((partialNextWeek.nextWeek.front().date == gregorianDate(9999, 12, 31)));

    const UpcomingBirthdaySchedule partialCurrentWeek =
        buildSchedule(
            {
                birthday("12-30", u"Today"),
                birthday("12-31", u"Partial current week"),
                birthday("01-01", u"Beyond maximum")
            },
            {9999, 12, 30}
            );

    QCOMPARE(partialCurrentWeek.today.size(), std::size_t{1});
    QCOMPARE(partialCurrentWeek.thisWeek.size(), std::size_t{1});
    QVERIFY((partialCurrentWeek.thisWeek.front().date == gregorianDate(9999, 12, 31)));
    QVERIFY(partialCurrentWeek.nextWeek.empty());

    const UpcomingBirthdaySchedule maxYearLeapDay = buildSchedule(
        {birthday("02-29", u"Leap Day")},
        {9999, 2, 22}
        );
    QCOMPARE(maxYearLeapDay.thisWeek.size(), std::size_t{1});
    QVERIFY((maxYearLeapDay.thisWeek.front().date == gregorianDate(9999, 2, 28)));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
invalidReferenceDateReturnsEmptySchedule()
{
    const std::vector<UpcomingBirthdayCandidate> candidates{
        birthday("08-18", u"Valid birthday")
    };

    QVERIFY(buildSchedule(candidates, {0, 8, 18}).isEmpty());
    QVERIFY(buildSchedule(candidates, {10000, 8, 18}).isEmpty());
    QVERIFY(buildSchedule(candidates, {2026, 2, 29}).isEmpty());
}

QTEST_APPLESS_MAIN(NextApplicationUpcomingBirthdayScheduleUseCaseTests)

#include "next_application_upcoming_birthday_schedule_use_case_tests.moc"
