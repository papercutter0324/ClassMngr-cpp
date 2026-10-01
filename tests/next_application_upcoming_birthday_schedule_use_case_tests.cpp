#include "next/application/upcoming_birthday_schedule_use_case.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <utility>

using namespace ClassMngr::Next::Application;

namespace
{

UpcomingBirthdayCandidate birthday(
    std::string monthDay,
    std::u16string displayName,
    const UpcomingBirthdayStaffGroup group =
        UpcomingBirthdayStaffGroup::KoreanTeacher
    )
{
    return {
        .birthdayMonthDay = std::move(monthDay),
        .displayName = std::move(displayName),
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
    void omitsInvalidBirthdaysAndBlankNames();
    void preservesNonAsciiUtf16DisplayName();
    void invalidReferenceDateReturnsEmptySchedule();
};

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
bucketsTodayThisWeekAndNextWeekThroughSunday()
{
    const UpcomingBirthdaySchedule schedule =
        UpcomingBirthdayScheduleUseCase::build(
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
    QVERIFY((schedule.today.front().date == UpcomingBirthdayDate{2026, 8, 18}));
    QCOMPARE(schedule.today.front().displayName, std::u16string(u"Today"));

    QCOMPARE(schedule.thisWeek.size(), std::size_t{1});
    QVERIFY((schedule.thisWeek.front().date == UpcomingBirthdayDate{2026, 8, 23}));
    QCOMPARE(
        schedule.thisWeek.front().displayName,
        std::u16string(u"This week Sunday")
        );

    QCOMPARE(schedule.nextWeek.size(), std::size_t{2});
    QVERIFY((schedule.nextWeek[0].date == UpcomingBirthdayDate{2026, 8, 24}));
    QVERIFY((schedule.nextWeek[1].date == UpcomingBirthdayDate{2026, 8, 30}));
    QVERIFY(schedule.nextWeek[1].group == UpcomingBirthdayStaffGroup::KoreanTeacher);
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
selectsOccurrencesAcrossCalendarYearRollover()
{
    const UpcomingBirthdaySchedule schedule =
        UpcomingBirthdayScheduleUseCase::build(
            {
                birthday("01-03", u"This week"),
                birthday("01-04", u"Next week"),
                birthday("01-10", u"Next week Sunday"),
                birthday("01-11", u"After range")
            },
            {2026, 12, 28}
            );

    QCOMPARE(schedule.thisWeek.size(), std::size_t{1});
    QVERIFY((schedule.thisWeek.front().date == UpcomingBirthdayDate{2027, 1, 3}));
    QCOMPARE(schedule.nextWeek.size(), std::size_t{2});
    QVERIFY((schedule.nextWeek[0].date == UpcomingBirthdayDate{2027, 1, 4}));
    QVERIFY((schedule.nextWeek[1].date == UpcomingBirthdayDate{2027, 1, 10}));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
mapsLeapDayToFebruaryTwentyEighthInNonLeapYear()
{
    const UpcomingBirthdaySchedule schedule =
        UpcomingBirthdayScheduleUseCase::build(
            {birthday("02-29", u"Leap Day")},
            {2027, 2, 22}
            );

    QCOMPARE(schedule.thisWeek.size(), std::size_t{1});
    QVERIFY((schedule.thisWeek.front().date == UpcomingBirthdayDate{2027, 2, 28}));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
omitsInvalidBirthdaysAndBlankNames()
{
    const UpcomingBirthdaySchedule schedule =
        UpcomingBirthdayScheduleUseCase::build(
            {
                birthday("02-30", u"Invalid day"),
                birthday("13-01", u"Invalid month"),
                birthday("2-03", u"Non-padded month"),
                birthday("08-18", u""),
                birthday("08-18", u"\u00A0 \t"),
                birthday("08-18", u"Visible")
            },
            {2026, 8, 18}
            );

    QCOMPARE(schedule.today.size(), std::size_t{1});
    QCOMPARE(schedule.today.front().displayName, std::u16string(u"Visible"));
}

void NextApplicationUpcomingBirthdayScheduleUseCaseTests::
preservesNonAsciiUtf16DisplayName()
{
    const UpcomingBirthdaySchedule schedule =
        UpcomingBirthdayScheduleUseCase::build(
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
invalidReferenceDateReturnsEmptySchedule()
{
    const UpcomingBirthdaySchedule schedule =
        UpcomingBirthdayScheduleUseCase::build(
            {birthday("08-18", u"Valid birthday")},
            {2026, 2, 29}
            );

    QVERIFY(schedule.isEmpty());
}

QTEST_APPLESS_MAIN(NextApplicationUpcomingBirthdayScheduleUseCaseTests)

#include "next_application_upcoming_birthday_schedule_use_case_tests.moc"
