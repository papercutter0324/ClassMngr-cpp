#include "next/application/schedule_testing_assignment_read_query.h"

#include <QtTest/QtTest>

#include <limits>
#include <string>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

static_assert(std::is_same_v<
    decltype(ScheduleTestingAssignmentReadRow::assignmentKey),
    ScheduleTestingAssignmentKey
    >);

Domain::ClassId typedClassId(const std::string& value)
{
    return Domain::ClassId::fromString(value).value();
}

class RecordingScheduleTestingAssignmentReadPort final
    : public ScheduleTestingAssignmentReadPort
{
public:
    [[nodiscard]] ScheduleTestingAssignmentReadResult readTestingAssignments(
        const ScheduleTestingAssignmentReadQuery& query
        ) const override
    {
        ++readCount;
        lastQuery = query;
        return result;
    }

    mutable int readCount = 0;
    mutable ScheduleTestingAssignmentReadQuery lastQuery;
    ScheduleTestingAssignmentReadResult result =
        ScheduleTestingAssignmentReadResult::success({});
};

}

class NextApplicationScheduleTestingAssignmentReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void keyPreservesExactPairValuesAndEquality();
    void preservesOrderedRawAssignmentsAndDisplayFields();
    void preservesAssignmentsWithoutSpecialClassMetadata();
    void acceptsAnEmptySuccessfulSnapshot();
    void acceptsMaximumPositiveIntClassId();
    void rejectsInvalidClassIdentifiers();
    void propagatesStructuredReadFailures();
};

void NextApplicationScheduleTestingAssignmentReadQueryTests::
keyPreservesExactPairValuesAndEquality()
{
    const ScheduleTestingAssignmentKey exact(u"Monday ", u"09:05 ");
    const ScheduleTestingAssignmentKey samePair(u"Monday ", u"09:05 ");
    const ScheduleTestingAssignmentKey differentDay(u"Monday", u"09:05 ");
    const ScheduleTestingAssignmentKey differentTime(u"Monday ", u"09:05");
    const ScheduleTestingAssignmentKey empty;

    QCOMPARE(exact.day(), std::u16string(u"Monday "));
    QCOMPARE(exact.startTime(), std::u16string(u"09:05 "));
    QVERIFY(exact == samePair);
    QVERIFY(!(exact == differentDay));
    QVERIFY(!(exact == differentTime));
    QVERIFY(empty.day().empty());
    QVERIFY(empty.startTime().empty());
}

void NextApplicationScheduleTestingAssignmentReadQueryTests::
preservesOrderedRawAssignmentsAndDisplayFields()
{
    RecordingScheduleTestingAssignmentReadPort port;
    const ScheduleTestingAssignmentReadQuery query;
    const ScheduleTestingAssignmentReadSnapshot expected{
        .rows = {
            {
                .assignmentKey = ScheduleTestingAssignmentKey(
                    u"Monday ",
                    u"09:05 "
                    ),
                .room = u"Room 4",
                .classId = typedClassId("-1")
            },
            {
                .assignmentKey = ScheduleTestingAssignmentKey(
                    u"Tuesday",
                    u"10:00"
                    ),
                .room = u"",
                .classId = typedClassId("42"),
                .specialClass = ScheduleTestingAssignmentSpecialClass{
                    .name = u"Oral Testing",
                    .teacherKoreanName = u"김선생",
                    .teacherEnglishName = u"Teacher Kim",
                    .teacherPreferredName = u"Kim",
                    .room = u"Library",
                    .grade = u"M2",
                    .level = u"Mixed (High)",
                    .classColor = u"#123456",
                    .fontColor = u"#FFFFFF"
                }
            },
            {
                .assignmentKey = ScheduleTestingAssignmentKey(
                    u"Wednesday",
                    u"11:00"
                    ),
                .room = u"",
                .classId = typedClassId("-1")
            }
        }
    };
    port.result = ScheduleTestingAssignmentReadResult::success(expected);

    const auto result =
        ScheduleTestingAssignmentReadQueryHandler::execute(query, port);

    QVERIFY(result);
    QCOMPARE(port.readCount, 1);
    QVERIFY(port.lastQuery == query);
    QCOMPARE(result.value(), expected);
    QCOMPARE(result.value().rows[0].classId.value(), std::string("-1"));
    QCOMPARE(result.value().rows[1].classId.value(), std::string("42"));
    QCOMPARE(
        result.value().rows[0].assignmentKey.day(),
        std::u16string(u"Monday ")
        );
    QCOMPARE(
        result.value().rows[0].assignmentKey.startTime(),
        std::u16string(u"09:05 ")
        );
    QCOMPARE(
        result.value().rows[1].assignmentKey.day(),
        std::u16string(u"Tuesday")
        );
    QCOMPARE(
        result.value().rows[1].assignmentKey.startTime(),
        std::u16string(u"10:00")
        );
    QCOMPARE(
        result.value().rows[1].specialClass->teacherPreferredName,
        std::u16string(u"Kim")
        );
}

void NextApplicationScheduleTestingAssignmentReadQueryTests::
preservesAssignmentsWithoutSpecialClassMetadata()
{
    RecordingScheduleTestingAssignmentReadPort port;
    port.result = ScheduleTestingAssignmentReadResult::success({
        .rows = {
            {
                .assignmentKey = ScheduleTestingAssignmentKey(
                    u"Monday",
                    u"09:00"
                    ),
                .room = u"Room 4",
                .classId = typedClassId("-1")
            },
            {
                .assignmentKey = ScheduleTestingAssignmentKey(
                    u"Tuesday",
                    u"10:00"
                    ),
                .classId = typedClassId("43")
            }
        }
    });

    const auto result =
        ScheduleTestingAssignmentReadQueryHandler::execute({}, port);

    QVERIFY(result);
    QCOMPARE(result.value().rows.size(), std::size_t(2));
    QCOMPARE(
        result.value().rows[0].assignmentKey.day(),
        std::u16string(u"Monday")
        );
    QCOMPARE(
        result.value().rows[1].assignmentKey.day(),
        std::u16string(u"Tuesday")
        );
    QCOMPARE(result.value().rows[1].classId.value(), std::string("43"));
    QVERIFY(!result.value().rows[1].specialClass);
}

void NextApplicationScheduleTestingAssignmentReadQueryTests::
acceptsAnEmptySuccessfulSnapshot()
{
    RecordingScheduleTestingAssignmentReadPort port;
    const auto result =
        ScheduleTestingAssignmentReadQueryHandler::execute({}, port);

    QVERIFY(result);
    QVERIFY(result.value().rows.empty());
    QCOMPARE(port.readCount, 1);
}

void NextApplicationScheduleTestingAssignmentReadQueryTests::
acceptsMaximumPositiveIntClassId()
{
    const std::string maximumClassId =
        std::to_string(std::numeric_limits<int>::max());
    RecordingScheduleTestingAssignmentReadPort port;
    port.result = ScheduleTestingAssignmentReadResult::success({
        .rows = {
            {
                .assignmentKey = ScheduleTestingAssignmentKey(
                    u"Monday",
                    u"09:00"
                    ),
                .classId = typedClassId(maximumClassId)
            }
        }
    });

    const auto result =
        ScheduleTestingAssignmentReadQueryHandler::execute({}, port);

    QVERIFY(result);
    QCOMPARE(result.value().rows[0].classId.value(), maximumClassId);
}

void NextApplicationScheduleTestingAssignmentReadQueryTests::
rejectsInvalidClassIdentifiers()
{
    const std::vector<std::string> invalidClassIds{
        "0",
        "-2",
        "-17",
        "-01",
        "not-an-id",
        "42tail",
        "+1",
        std::to_string(std::numeric_limits<int>::max()) + "0",
        std::to_string(std::numeric_limits<int>::min()) + "0"
    };
    for (const std::string& classId : invalidClassIds)
    {
        RecordingScheduleTestingAssignmentReadPort port;
        port.result = ScheduleTestingAssignmentReadResult::success({
            .rows = {
                {
                    .assignmentKey = ScheduleTestingAssignmentKey(
                        u"Monday",
                        u"09:00"
                        ),
                    .room = u"Room 4",
                    .classId = typedClassId(classId)
                }
            }
        });

        const auto result =
            ScheduleTestingAssignmentReadQueryHandler::execute({}, port);

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QCOMPARE(
            result.error().message,
            std::string("The schedule testing assignment contains an invalid class identifier.")
            );
        QVERIFY(!result.error().recoverable);
        QCOMPARE(port.readCount, 1);
    }
}

void NextApplicationScheduleTestingAssignmentReadQueryTests::
propagatesStructuredReadFailures()
{
    RecordingScheduleTestingAssignmentReadPort port;
    const Domain::OperationError readError{
        .code = Domain::ErrorCode::Technical,
        .message = "testing assignment repository failed",
        .recoverable = false
    };
    port.result = ScheduleTestingAssignmentReadResult::failure(readError);

    const auto result =
        ScheduleTestingAssignmentReadQueryHandler::execute({}, port);

    QVERIFY(!result);
    QVERIFY(result.error() == readError);
    QCOMPARE(port.readCount, 1);
}

QTEST_GUILESS_MAIN(
    NextApplicationScheduleTestingAssignmentReadQueryTests
    )

#include "next_application_schedule_testing_assignment_read_query_tests.moc"
