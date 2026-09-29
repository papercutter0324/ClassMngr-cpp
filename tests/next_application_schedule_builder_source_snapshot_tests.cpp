#include "next/application/schedule_builder_source_snapshot.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(std::string value)
{
    return *Domain::ClassId::fromString(std::move(value));
}

ScheduleBuilderSourceClass sourceClass(const std::string& id)
{
    return {
        .classId = classId(id)
    };
}

class RecordingScheduleBuilderSourceReadPort final
    : public ScheduleBuilderSourceReadPort
{
public:
    [[nodiscard]] ScheduleBuilderSourceResult readScheduleClasses(
        const ScheduleBuilderSourceQuery& query
        ) const override
    {
        ++readCount;
        lastQuery = query;
        return result;
    }

    mutable int readCount = 0;
    mutable ScheduleBuilderSourceQuery lastQuery;
    ScheduleBuilderSourceResult result =
        ScheduleBuilderSourceResult::success({});
};

}

class NextApplicationScheduleBuilderSourceSnapshotTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void preservesCompactFieldsAndSourceAndTimeOrder();
    void acceptsEmptySourceSnapshot();
    void propagatesReadErrorsAndRejectsInvalidIdentifiers();
};

void NextApplicationScheduleBuilderSourceSnapshotTests::
preservesCompactFieldsAndSourceAndTimeOrder()
{
    RecordingScheduleBuilderSourceReadPort port;
    const ScheduleBuilderSourceQuery query;
    ScheduleBuilderSourceClass first = sourceClass("43");
    first.teacherKoreanName = u" \uAE40\uC9C0 ";
    first.teacherEnglishName = u"  Teacher \U0001F9ED  ";
    first.teacherPreferredName = u" Preferred  ";
    first.roomNumber = u" Room 4 ";
    first.grade = u" E5 ";
    first.level = u" Athena ";
    first.classColor = u" #112233 ";
    first.fontColor = u" #445566 ";
    first.regularSchedule = {
        {u"  Monday ", u"4:00 PM", u"4:55 PM"},
        {u"Raw Day", u" invalid start \U0001F9ED", u" opaque end "}
    };
    first.intensiveSchedule = {
        {u" Friday ", u"09:00", u"09:50"},
        {u"  ", u"9:05 AM", u"bad end"}
    };

    ScheduleBuilderSourceClass second = sourceClass("42");
    second.teacherKoreanName = u"Unassigned";

    port.result = ScheduleBuilderSourceResult::success({
        .classes = {first, second}
    });

    const auto result = ScheduleBuilderSourceQueryHandler::execute(
        query,
        port
        );

    QVERIFY(result);
    QCOMPARE(port.readCount, 1);
    QCOMPARE(result.value().classes.size(), std::size_t(2));
    QVERIFY(port.lastQuery == query);
    QVERIFY(result.value().classes[0] == first);
    QVERIFY(result.value().classes[1] == second);
    QCOMPARE(result.value().classes[0].teacherEnglishName,
             std::u16string(u"  Teacher \U0001F9ED  "));
    QCOMPARE(result.value().classes[0].teacherKoreanName,
             std::u16string(u" \uAE40\uC9C0 "));
    QCOMPARE(result.value().classes[0].regularSchedule[1].startTime,
             std::u16string(u" invalid start \U0001F9ED"));
    QCOMPARE(result.value().classes[0].intensiveSchedule[1].day,
             std::u16string(u"  "));
}

void NextApplicationScheduleBuilderSourceSnapshotTests::
acceptsEmptySourceSnapshot()
{
    RecordingScheduleBuilderSourceReadPort port;
    const auto result = ScheduleBuilderSourceQueryHandler::execute({}, port);

    QVERIFY(result);
    QVERIFY(result.value().classes.empty());
    QCOMPARE(port.readCount, 1);
}

void NextApplicationScheduleBuilderSourceSnapshotTests::
propagatesReadErrorsAndRejectsInvalidIdentifiers()
{
    RecordingScheduleBuilderSourceReadPort port;
    const Domain::OperationError readError{
        .code = Domain::ErrorCode::Technical,
        .message = "schedule source unavailable",
        .recoverable = false
    };
    port.result = ScheduleBuilderSourceResult::failure(readError);

    auto result = ScheduleBuilderSourceQueryHandler::execute({}, port);
    QVERIFY(!result);
    QVERIFY(result.error() == readError);

    ScheduleBuilderSourceClass invalid = sourceClass("042");
    port.result = ScheduleBuilderSourceResult::success({
        .classes = {invalid}
    });
    result = ScheduleBuilderSourceQueryHandler::execute({}, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    ScheduleBuilderSourceClass duplicate = sourceClass("43");
    port.result = ScheduleBuilderSourceResult::success({
        .classes = {duplicate, duplicate}
    });
    result = ScheduleBuilderSourceQueryHandler::execute({}, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
}

QTEST_APPLESS_MAIN(NextApplicationScheduleBuilderSourceSnapshotTests)

#include "next_application_schedule_builder_source_snapshot_tests.moc"
