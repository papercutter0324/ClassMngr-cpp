#include "next/application/schedule_builder_source_snapshot.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

using ScheduleText = ScheduleBuilderSourceScheduleText;

static_assert(std::is_default_constructible_v<ScheduleText>);
static_assert(std::is_constructible_v<ScheduleText, std::u16string>);
static_assert(!std::is_convertible_v<std::u16string, ScheduleText>);
static_assert(!std::is_convertible_v<ScheduleText, std::u16string>);
static_assert(std::is_same_v<
              decltype(std::declval<ScheduleBuilderSourceScheduleRow>().day),
              ScheduleText>);

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

ScheduleBuilderSourceScheduleRow scheduleRow(
    std::u16string day,
    std::u16string startTime,
    std::u16string endTime
    )
{
    return {
        ScheduleText(std::move(day)),
        ScheduleText(std::move(startTime)),
        ScheduleText(std::move(endTime))
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
    void scheduleTextIsRawAndDefaultsToEmpty();
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
        scheduleRow(u"  Monday ", u"4:00 PM", u"4:55 PM"),
        scheduleRow(u"Raw Day", u" invalid start \U0001F9ED", u" opaque end ")
    };
    first.intensiveSchedule = {
        scheduleRow(u" Friday ", u"09:00", u"09:50"),
        scheduleRow(u"  ", u"9:05 AM", u"bad end")
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
    QCOMPARE(result.value().classes[0].regularSchedule[1].startTime.value(),
             std::u16string(u" invalid start \U0001F9ED"));
    QCOMPARE(result.value().classes[0].intensiveSchedule[1].day.value(),
             std::u16string(u"  "));
}

void NextApplicationScheduleBuilderSourceSnapshotTests::
scheduleTextIsRawAndDefaultsToEmpty()
{
    const ScheduleText empty;
    QVERIFY(empty.value().empty());

    const ScheduleText raw(std::u16string(u" \U0001F9ED invalid "));
    QCOMPARE(raw.value(), std::u16string(u" \U0001F9ED invalid "));
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
    QCOMPARE(port.readCount, 1);

    port.readCount = 0;
    ScheduleBuilderSourceClass invalid = sourceClass("042");
    port.result = ScheduleBuilderSourceResult::success({
        .classes = {invalid}
    });
    result = ScheduleBuilderSourceQueryHandler::execute({}, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(result.error().message,
             std::string("A schedule source returned an invalid class ID."));
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.readCount, 1);

    port.readCount = 0;
    invalid = sourceClass("42x");
    port.result = ScheduleBuilderSourceResult::success({
        .classes = {invalid}
    });
    result = ScheduleBuilderSourceQueryHandler::execute({}, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(result.error().message,
             std::string("A schedule source returned an invalid class ID."));
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.readCount, 1);

    port.readCount = 0;
    ScheduleBuilderSourceClass duplicate = sourceClass("43");
    port.result = ScheduleBuilderSourceResult::success({
        .classes = {duplicate, duplicate}
    });
    result = ScheduleBuilderSourceQueryHandler::execute({}, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(result.error().message,
             std::string("A schedule source returned duplicate class IDs."));
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.readCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationScheduleBuilderSourceSnapshotTests)

#include "next_application_schedule_builder_source_snapshot_tests.moc"
