#include "next/application/class_co_teacher_page_read_query.h"
#include "next/application/schedule_testing_assignment_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

static_assert(!std::is_same_v<
    ClassCoTeacherPageScheduleSlot,
    ScheduleTestingAssignmentKey
    >);
static_assert(!std::is_convertible_v<
    ClassCoTeacherPageScheduleSlot,
    ScheduleTestingAssignmentKey
    >);
static_assert(!std::is_convertible_v<
    ScheduleTestingAssignmentKey,
    ClassCoTeacherPageScheduleSlot
    >);
static_assert(std::is_same_v<
    decltype(ClassCoTeacherPageFields::regularSchedule)::value_type,
    ClassCoTeacherPageScheduleSlot
    >);

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

Domain::TeacherId teacherId(const std::string& value)
{
    return *Domain::TeacherId::fromString(value);
}

class RecordingReadPort final : public ClassCoTeacherPageReadPort
{
public:
    [[nodiscard]] ClassCoTeacherPageReadResult readClassCoTeacherPage(
        const Domain::ClassId& request
        ) const override
    {
        ++callCount;
        requests.push_back(request);
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requests;
    ClassCoTeacherPageReadResult result = ClassCoTeacherPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Unconfigured read port",
        .recoverable = false
    });
};

ClassCoTeacherPageReadSnapshot snapshot(
    const Domain::ClassId& id,
    Domain::Result<ClassCoTeacherPageFields> classFields,
    Domain::Result<std::u16string> teacherName
    )
{
    return {id, std::move(classFields), std::move(teacherName)};
}

}

class NextApplicationClassCoTeacherPageReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void scheduleSlotPreservesExactPairAndEquality();
    void rejectsNonCanonicalClassIdsBeforePortCall();
    void callsPortOnceWithTypedIdAndPreservesPageFields();
    void propagatesPortErrorAndRejectsIdentityMismatch();
};

void NextApplicationClassCoTeacherPageReadQueryTests::
scheduleSlotPreservesExactPairAndEquality()
{
    const ClassCoTeacherPageScheduleSlot exact(
        u" Monday ",
        u"not a time:99 "
        );
    const ClassCoTeacherPageScheduleSlot samePair(
        u" Monday ",
        u"not a time:99 "
        );
    const ClassCoTeacherPageScheduleSlot differentDay(
        u"Monday",
        u"not a time:99 "
        );
    const ClassCoTeacherPageScheduleSlot differentTime(
        u" Monday ",
        u"not a time:99"
        );
    const ClassCoTeacherPageScheduleSlot empty;

    QCOMPARE(exact.day(), std::u16string(u" Monday "));
    QCOMPARE(exact.startTime(), std::u16string(u"not a time:99 "));
    QVERIFY(exact == samePair);
    QVERIFY(!(exact == differentDay));
    QVERIFY(!(exact == differentTime));
    QVERIFY(empty.day().empty());
    QVERIFY(empty.startTime().empty());

    ClassCoTeacherPageFields fields;
    fields.regularSchedule = {
        ClassCoTeacherPageScheduleSlot(u" Wednesday ", u"raw slot "),
        ClassCoTeacherPageScheduleSlot(u"not-a-day", u"99:99")
    };
    QCOMPARE(fields.regularSchedule.size(), std::size_t(2));
    QCOMPARE(fields.regularSchedule[0].day(), std::u16string(u" Wednesday "));
    QCOMPARE(fields.regularSchedule[0].startTime(), std::u16string(u"raw slot "));
    QCOMPARE(fields.regularSchedule[1].day(), std::u16string(u"not-a-day"));
    QCOMPARE(fields.regularSchedule[1].startTime(), std::u16string(u"99:99"));
}

void NextApplicationClassCoTeacherPageReadQueryTests::
rejectsNonCanonicalClassIdsBeforePortCall()
{
    RecordingReadPort port;
    const ClassCoTeacherPageReadQuery query(port);
    const std::vector<Domain::ClassId> invalid{
        classId("0"),
        classId("-1"),
        classId("01"),
        classId("+2"),
        classId(" 3"),
        classId("42x"),
        classId("2147483648")
    };

    for (const auto& id : invalid)
    {
        const auto result = query.execute(id);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("Class ID must be a canonical positive integer.")
            );
        QVERIFY(!result.error().recoverable);
    }
    QCOMPARE(port.callCount, 0);
    QVERIFY(port.requests.empty());
}

void NextApplicationClassCoTeacherPageReadQueryTests::
callsPortOnceWithTypedIdAndPreservesPageFields()
{
    RecordingReadPort port;
    const auto requested = classId("42");
    ClassCoTeacherPageFields fields;
    fields.selectedTeacherId = teacherId("17");
    fields.classGrade = u"E4";
    fields.classLevel = u"Theseus";
    fields.regularSchedule = {
        ClassCoTeacherPageScheduleSlot(u"Monday", u"4:00 PM"),
        ClassCoTeacherPageScheduleSlot(u"Wednesday", u"4:00 PM")
    };
    const auto expected = fields;
    port.result = ClassCoTeacherPageReadResult::success(snapshot(
        requested,
        Domain::Result<ClassCoTeacherPageFields>::success(std::move(fields)),
        Domain::Result<std::u16string>::success(u"Preferred Teacher")
        ));
    const ClassCoTeacherPageReadQuery query(port);

    const auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requests.front() == requested);
    QVERIFY(result.value().classId == requested);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().classFields.value() == expected);
    QVERIFY(result.value().classFields.value().selectedTeacherId.has_value());
    QVERIFY(result.value().classFields.value().selectedTeacherId.value()
        == teacherId("17"));
    QVERIFY(result.value().teacherDisplayName);
    QVERIFY(result.value().teacherDisplayName.value() == u"Preferred Teacher");

    port.result = ClassCoTeacherPageReadResult::success(snapshot(
        requested,
        Domain::Result<ClassCoTeacherPageFields>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "class source failed",
            .recoverable = true
        }),
        Domain::Result<std::u16string>::success(u"Teacher source still available")
        ));
    const auto partial = query.execute(requested);
    QVERIFY(partial);
    QVERIFY(!partial.value().classFields);
    QVERIFY(partial.value().teacherDisplayName);
    QVERIFY(partial.value().teacherDisplayName.value()
        == u"Teacher source still available");
    QCOMPARE(port.callCount, 2);
}

void NextApplicationClassCoTeacherPageReadQueryTests::
propagatesPortErrorAndRejectsIdentityMismatch()
{
    RecordingReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "read failed",
        .recoverable = false
    };
    port.result = ClassCoTeacherPageReadResult::failure(expected);
    const ClassCoTeacherPageReadQuery query(port);
    auto result = query.execute(classId("7"));
    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);

    port.result = ClassCoTeacherPageReadResult::success(snapshot(
        classId("8"),
        Domain::Result<ClassCoTeacherPageFields>::success({}),
        Domain::Result<std::u16string>::success({})
        ));
    result = query.execute(classId("7"));
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        result.error().message,
        std::string(
            "A co-teacher page read returned a different class identifier."
            )
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 2);
}

QTEST_APPLESS_MAIN(NextApplicationClassCoTeacherPageReadQueryTests)

#include "next_application_class_co_teacher_page_read_query_tests.moc"
