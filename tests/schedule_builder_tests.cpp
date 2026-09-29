#include "features/schedule/ui/schedule_builder.h"

#include <QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

ScheduleBuilderSourceClass scheduleClass(const int id)
{
    return {
        .classId = *Domain::ClassId::fromString(std::to_string(id))
    };
}

ScheduleBuilderSourceScheduleRow time(
    const std::u16string& day,
    const std::u16string& start,
    const std::u16string& end
    )
{
    return {day, start, end};
}

ScheduleBuildResult build(
    const ScheduleBuilderSourceSnapshot& source,
    const bool intensive,
    const QStringList& visibleDays
    )
{
    const ScheduleBuilder builder;
    return builder.build(source, intensive, visibleDays);
}

}

class ScheduleBuilderTests final : public QObject
{
    Q_OBJECT

private slots:
    void preservesSourceAndCollisionAppendOrderAndEntryFields();
    void appliesBlankDayAndVisibleDayRules();
    void retainsAcceptedTimeFormatsAndSkipsInvalidStartsOnly();
    void retainsEarliestHourOffsetAnd55EndingRules();
    void intensiveModeUsesIntensiveRowsAndResetsOffsets();
    void emptySnapshotStillBuildsARegularScheduleGrid();
};

void ScheduleBuilderTests::
preservesSourceAndCollisionAppendOrderAndEntryFields()
{
    ScheduleBuilderSourceClass first = scheduleClass(42);
    first.teacherKoreanName = u"\uAE40 \uC9C0";
    first.teacherEnglishName = u" Teacher 42 ";
    first.teacherPreferredName = u"Preferred 42";
    first.roomNumber = u" Room 4 ";
    first.grade = u"E4";
    first.level = u"Hercules";
    first.classColor = u"#123456";
    first.fontColor = u"#ABCDEF";
    first.regularSchedule = {
        time(u"Monday", u"4:00 PM", u"4:50 PM"),
        time(u"Monday", u"4:00PM", u"bad end")
    };

    ScheduleBuilderSourceClass second = scheduleClass(43);
    second.teacherEnglishName = u"Teacher 43";
    second.regularSchedule = {
        time(u"Monday", u"16:00", u"16:50")
    };

    const ScheduleBuildResult result = build(
        {.classes = {first, second}},
        false,
        {QStringLiteral("Monday")}
        );

    QCOMPARE(result.rows.size(), 6);
    QCOMPARE(result.rows.first().label, QStringLiteral("16:00"));
    QCOMPARE(result.rows.last().label, QStringLiteral("21:00"));
    QCOMPARE(result.schedule[QStringLiteral("Monday")]
                 [QStringLiteral("16:00")]
                     .size(),
             3);

    const QList<ScheduleEntry> entries =
        result.schedule.value(QStringLiteral("Monday"))
            .value(QStringLiteral("16:00"));
    QCOMPARE(entries[0].classId, 42);
    QCOMPARE(entries[0].teacherKr, QString::fromStdU16String(u"\uAE40 \uC9C0"));
    QCOMPARE(entries[0].teacherEn, QStringLiteral(" Teacher 42 "));
    QCOMPARE(entries[0].teacherPreferredName, QStringLiteral("Preferred 42"));
    QCOMPARE(entries[0].roomNumber, QStringLiteral(" Room 4 "));
    QCOMPARE(entries[0].classGrade, QStringLiteral("E4"));
    QCOMPARE(entries[0].classLevel, QStringLiteral("Hercules"));
    QCOMPARE(entries[0].classColor, QStringLiteral("#123456"));
    QCOMPARE(entries[0].fontColor, QStringLiteral("#ABCDEF"));
    QCOMPARE(entries[1].classId, 42);
    QCOMPARE(entries[2].classId, 43);
    QCOMPARE(entries[2].classColor, QStringLiteral("#FFFFFF"));
    QCOMPARE(entries[2].fontColor, QStringLiteral("#000000"));
}

void ScheduleBuilderTests::appliesBlankDayAndVisibleDayRules()
{
    ScheduleBuilderSourceClass source = scheduleClass(42);
    source.regularSchedule = {
        time(u"  ", u"4:00 PM", u"4:50 PM"),
        time(u" Tuesday ", u"04:00PM", u"04:50PM"),
        time(u"tuesday", u"4:00 PM", u"4:50 PM"),
        time(u"Friday", u"4:00 PM", u"4:50 PM")
    };

    const ScheduleBuildResult result = build(
        {.classes = {source}},
        false,
        {QStringLiteral("Monday"), QStringLiteral("Tuesday")}
        );

    QCOMPARE(result.schedule.keys(),
             QStringList({QStringLiteral("Monday"), QStringLiteral("Tuesday")}));
    QCOMPARE(result.schedule[QStringLiteral("Monday")]
                 [QStringLiteral("16:00")]
                     .size(),
             1);
    QCOMPARE(result.schedule[QStringLiteral("Tuesday")]
                 [QStringLiteral("16:00")]
                     .size(),
             1);
    QVERIFY(!result.schedule.contains(QStringLiteral("Friday")));
}

void ScheduleBuilderTests::
retainsAcceptedTimeFormatsAndSkipsInvalidStartsOnly()
{
    ScheduleBuilderSourceClass source = scheduleClass(42);
    source.regularSchedule = {
        time(u"Monday", u"4:00 PM", u"invalid end"),
        time(u"Monday", u"4:00PM", u""),
        time(u"Monday", u"04:00 PM", u"not a time"),
        time(u"Monday", u"04:00PM", u"opaque"),
        time(u"Monday", u"4:00", u"4:50 PM"),
        time(u"Monday", u"04:00", u"4:50 PM"),
        time(u"Monday", u"4:00:00", u"4:50 PM"),
        time(u"Monday", u"04:00:00", u"4:50 PM"),
        time(u"Monday", u"invalid start", u"4:55 PM")
    };

    const ScheduleBuildResult result = build(
        {.classes = {source}},
        false,
        {QStringLiteral("Monday")}
        );

    QCOMPARE(result.schedule[QStringLiteral("Monday")]
                 [QStringLiteral("16:00")]
                     .size(),
             4);
    QCOMPARE(result.schedule[QStringLiteral("Monday")]
                 [QStringLiteral("04:00")]
                     .size(),
             4);
    QVERIFY(!result.uses55Endings);
}

void ScheduleBuilderTests::retainsEarliestHourOffsetAnd55EndingRules()
{
    ScheduleBuilderSourceClass source = scheduleClass(42);
    source.regularSchedule = {
        time(u"Monday", u"5:05 PM", u"5:50 PM"),
        time(u"Tuesday", u"4:55 PM", u"4:55 PM"),
        time(u"Wednesday", u"3:05 PM", u"invalid end")
    };

    const ScheduleBuildResult result = build(
        {.classes = {source}},
        false,
        {QStringLiteral("Monday"), QStringLiteral("Tuesday"),
         QStringLiteral("Wednesday")}
        );

    QCOMPARE(result.scheduleOffset, 55);
    QVERIFY(result.uses55Endings);
    QCOMPARE(result.rows.size(), 7);
    QCOMPARE(result.rows.first().label, QStringLiteral("14:55"));
    QCOMPARE(result.rows.last().label, QStringLiteral("20:55"));
}

void ScheduleBuilderTests::intensiveModeUsesIntensiveRowsAndResetsOffsets()
{
    ScheduleBuilderSourceClass source = scheduleClass(42);
    source.regularSchedule = {
        time(u"Monday", u"4:00 PM", u"4:55 PM")
    };
    source.intensiveSchedule = {
        time(u"  Friday  ", u"9:05 AM", u"9:55 AM")
    };

    const ScheduleBuildResult result = build(
        {.classes = {source}},
        true,
        {QStringLiteral("Friday"), QStringLiteral("Monday")}
        );

    QCOMPARE(result.rows.size(), 13);
    QCOMPARE(result.rows.first().label, QStringLiteral("09:00"));
    QCOMPARE(result.rows.last().label, QStringLiteral("21:00"));
    QCOMPARE(result.scheduleOffset, 0);
    QVERIFY(!result.uses55Endings);
    QCOMPARE(result.schedule[QStringLiteral("Friday")]
                 [QStringLiteral("09:05")]
                     .size(),
             1);
    QVERIFY(result.schedule[QStringLiteral("Monday")].isEmpty());
}

void ScheduleBuilderTests::emptySnapshotStillBuildsARegularScheduleGrid()
{
    const ScheduleBuildResult result = build(
        {},
        false,
        {QStringLiteral("Monday")}
        );

    QCOMPARE(result.days, QStringList{QStringLiteral("Monday")});
    QCOMPARE(result.rows.size(), 6);
    QVERIFY(result.schedule.contains(QStringLiteral("Monday")));
    QVERIFY(result.schedule[QStringLiteral("Monday")].isEmpty());
}

QTEST_APPLESS_MAIN(ScheduleBuilderTests)

#include "schedule_builder_tests.moc"
