#include "next/application/schedule_testing_assignment_read_query.h"

#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

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
    void preservesOrderedRawAssignmentsAndDisplayFields();
    void preservesAssignmentsWithoutSpecialClassMetadata();
    void acceptsAnEmptySuccessfulSnapshot();
    void propagatesStructuredReadFailures();
};

void NextApplicationScheduleTestingAssignmentReadQueryTests::
preservesOrderedRawAssignmentsAndDisplayFields()
{
    RecordingScheduleTestingAssignmentReadPort port;
    const ScheduleTestingAssignmentReadQuery query;
    const ScheduleTestingAssignmentReadSnapshot expected{
        .rows = {
            {
                .day = u"Monday ",
                .startTime = u"09:05 ",
                .room = u"Room 4",
                .classId = -1
            },
            {
                .day = u"Tuesday",
                .startTime = u"10:00",
                .room = u"",
                .classId = 42,
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
                .day = u"Wednesday",
                .startTime = u"11:00",
                .room = u"",
                .classId = -1
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
    QCOMPARE(result.value().rows[0].day, std::u16string(u"Monday "));
    QCOMPARE(result.value().rows[0].startTime, std::u16string(u"09:05 "));
    QCOMPARE(result.value().rows[1].day, std::u16string(u"Tuesday"));
    QCOMPARE(result.value().rows[1].startTime, std::u16string(u"10:00"));
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
                .day = u"Monday",
                .startTime = u"09:00",
                .room = u"Room 4",
                .classId = -1
            },
            {
                .day = u"Tuesday",
                .startTime = u"10:00",
                .classId = 43
            }
        }
    });

    const auto result =
        ScheduleTestingAssignmentReadQueryHandler::execute({}, port);

    QVERIFY(result);
    QCOMPARE(result.value().rows.size(), std::size_t(2));
    QCOMPARE(result.value().rows[0].day, std::u16string(u"Monday"));
    QCOMPARE(result.value().rows[1].day, std::u16string(u"Tuesday"));
    QCOMPARE(result.value().rows[1].classId, 43);
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
