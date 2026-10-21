#include "next/application/sub_prep_vacation_selection_policy.h"

#include <QtTest>

#include <cstddef>
#include <vector>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

SubPrepCalendarEventInterval interval(
    const CalendarEventType eventType,
    const char* startDate,
    const char* endDate
    )
{
    return {
        eventType,
        CalendarEventDate(startDate),
        CalendarEventDate(endDate)
    };
}

QStringList isoDates(const std::vector<GregorianDate>& dates)
{
    QStringList result;
    for (const GregorianDate& current : dates)
    {
        result.append(QString::fromStdString(current.toIsoString()));
    }
    return result;
}

} // namespace

class NextApplicationSubPrepVacationSelectionPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void selectsEarliestBlockAfterSorting();
    void lookaheadIncludesDayTwentyEightButExcludesDayTwentyNine();
    void returnsWholeEligibleBlockOutsideReferenceAndLookahead();
    void holidayBridgesOnlyFullyCoveredWeekdays();
    void weekendOnlyGapConnectsAndKeepsPreReferenceDates();
    void ignoresOtherEventsAndInvalidRanges();
    void invalidReferenceAndNoEligibleBlockReturnEmpty();
};

void NextApplicationSubPrepVacationSelectionPolicyTests::
selectsEarliestBlockAfterSorting()
{
    const auto selected = selectNextSubPrepVacationBlock(
        {
            interval(CalendarEventType::Vacation, "2026-07-20", "2026-07-21"),
            interval(CalendarEventType::Vacation, "2026-07-06", "2026-07-07"),
            interval(CalendarEventType::Vacation, "2026-07-13", "2026-07-14")
        },
        CalendarEventDate("2026-07-01")
        );

    QCOMPARE(
        isoDates(selected),
        QStringList({QStringLiteral("2026-07-06"), QStringLiteral("2026-07-07")})
        );
}

void NextApplicationSubPrepVacationSelectionPolicyTests::
lookaheadIncludesDayTwentyEightButExcludesDayTwentyNine()
{
    const auto dayTwentyEight = selectNextSubPrepVacationBlock(
        {interval(CalendarEventType::Vacation, "2026-08-03", "2026-08-03")},
        CalendarEventDate("2026-07-06")
        );
    const auto dayTwentyNine = selectNextSubPrepVacationBlock(
        {interval(CalendarEventType::Vacation, "2026-08-04", "2026-08-04")},
        CalendarEventDate("2026-07-06")
        );

    QCOMPARE(isoDates(dayTwentyEight), QStringList({QStringLiteral("2026-08-03")}));
    QVERIFY(dayTwentyNine.empty());
}

void NextApplicationSubPrepVacationSelectionPolicyTests::
returnsWholeEligibleBlockOutsideReferenceAndLookahead()
{
    const auto selected = selectNextSubPrepVacationBlock(
        {
            interval(CalendarEventType::Vacation, "2026-07-03", "2026-07-03"),
            interval(CalendarEventType::Vacation, "2026-07-06", "2026-08-04")
        },
        CalendarEventDate("2026-07-06")
        );

    QCOMPARE(selected.size(), std::size_t(23));
    const QStringList selectedIsoDates = isoDates(selected);
    QCOMPARE(selectedIsoDates.first(), QStringLiteral("2026-07-03"));
    QCOMPARE(selectedIsoDates.last(), QStringLiteral("2026-08-04"));
}

void NextApplicationSubPrepVacationSelectionPolicyTests::
holidayBridgesOnlyFullyCoveredWeekdays()
{
    const auto bridged = selectNextSubPrepVacationBlock(
        {
            interval(CalendarEventType::Vacation, "2026-07-06", "2026-07-06"),
            interval(CalendarEventType::Holiday, "2026-07-07", "2026-07-08"),
            interval(CalendarEventType::Vacation, "2026-07-09", "2026-07-09")
        },
        CalendarEventDate("2026-07-01")
        );
    const auto splitByUncoveredWeekday = selectNextSubPrepVacationBlock(
        {
            interval(CalendarEventType::Vacation, "2026-07-06", "2026-07-06"),
            interval(CalendarEventType::Holiday, "2026-07-07", "2026-07-07"),
            interval(CalendarEventType::Vacation, "2026-07-09", "2026-07-09")
        },
        CalendarEventDate("2026-07-01")
        );

    QCOMPARE(
        isoDates(bridged),
        QStringList({QStringLiteral("2026-07-06"), QStringLiteral("2026-07-09")})
        );
    QCOMPARE(isoDates(splitByUncoveredWeekday), QStringList({QStringLiteral("2026-07-06")}));
}

void NextApplicationSubPrepVacationSelectionPolicyTests::
weekendOnlyGapConnectsAndKeepsPreReferenceDates()
{
    const auto selected = selectNextSubPrepVacationBlock(
        {
            interval(CalendarEventType::Vacation, "2026-07-03", "2026-07-03"),
            interval(CalendarEventType::Vacation, "2026-07-06", "2026-07-06")
        },
        CalendarEventDate("2026-07-06")
        );

    QCOMPARE(
        isoDates(selected),
        QStringList({
            QStringLiteral("2026-07-03"),
            QStringLiteral("2026-07-06")
        })
        );
}

void NextApplicationSubPrepVacationSelectionPolicyTests::
ignoresOtherEventsAndInvalidRanges()
{
    const auto selected = selectNextSubPrepVacationBlock(
        {
            interval(CalendarEventType::Workshop, "2026-07-06", "2026-07-10"),
            interval(CalendarEventType::Vacation, "invalid", "2026-07-10"),
            interval(CalendarEventType::Vacation, "2026-07-10", "2026-07-06"),
            interval(CalendarEventType::Vacation, "2026-07-13", "2026-07-13")
        },
        CalendarEventDate("2026-07-06")
        );

    QCOMPARE(isoDates(selected), QStringList({QStringLiteral("2026-07-13")}));
}

void NextApplicationSubPrepVacationSelectionPolicyTests::
invalidReferenceAndNoEligibleBlockReturnEmpty()
{
    const SubPrepCalendarEventIntervals futureVacation{
        interval(CalendarEventType::Vacation, "2026-08-04", "2026-08-04")
    };

    QVERIFY(
        selectNextSubPrepVacationBlock(
            futureVacation,
            CalendarEventDate("2026-07-06")
            ).empty()
        );
    QVERIFY(
        selectNextSubPrepVacationBlock(
            futureVacation,
            CalendarEventDate("not-a-date")
            ).empty()
        );
    QVERIFY(
        selectNextSubPrepVacationBlock(
            futureVacation,
            CalendarEventDate()
            ).empty()
        );
}

QTEST_APPLESS_MAIN(NextApplicationSubPrepVacationSelectionPolicyTests)

#include "next_application_sub_prep_vacation_selection_policy_tests.moc"
