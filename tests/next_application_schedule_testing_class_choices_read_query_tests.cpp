#include "next/application/schedule_testing_class_choices_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

class RecordingScheduleTestingClassChoicesReadPort final
    : public Application::ScheduleTestingClassChoicesReadPort
{
public:
    [[nodiscard]] Application::ScheduleTestingClassChoicesReadResult
    readTestingClassChoices(
        const Application::ScheduleTestingClassChoicesReadQuery& query
        ) const override
    {
        ++callCount;
        lastQuery = query;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<
        Application::ScheduleTestingClassChoicesReadQuery
        > lastQuery;
    Application::ScheduleTestingClassChoicesReadResult result =
        Application::ScheduleTestingClassChoicesReadResult::success({});
};

Domain::ClassId classId(const std::string& value)
{
    const auto parsed = Domain::ClassId::fromString(value);
    if (!parsed)
    {
        qFatal("Test class ID must have a typed representation.");
    }
    return *parsed;
}

}

class NextApplicationScheduleTestingClassChoicesReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void preservesOrderedChoicesAndEveryDisplayField();
    void preservesAnEmptySuccessfulSnapshot();
    void propagatesStructuredReadFailures();
};

void NextApplicationScheduleTestingClassChoicesReadQueryTests::
preservesOrderedChoicesAndEveryDisplayField()
{
    RecordingScheduleTestingClassChoicesReadPort port;
    const Application::ScheduleTestingClassChoicesReadQuery query;
    const Application::ScheduleTestingClassChoicesSnapshot expected{
        .choices = {
            {
                .classId = classId("42"),
                .name = u"Writing Lab",
                .grade = u"M2",
                .level = u"Mixed (High)",
                .room = u"Library"
            },
            {
                .classId = classId("7"),
                .name = u"Oral Review",
                .grade = u"E5",
                .level = u"Lower",
                .room = u"Room 204"
            }
        }
    };
    port.result =
        Application::ScheduleTestingClassChoicesReadResult::success(
            expected
            );

    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            query,
            port
            );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastQuery.has_value());
    QVERIFY(port.lastQuery.value() == query);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().choices.size(), std::size_t(2));
    QCOMPARE(result.value().choices[0].classId.value(), std::string("42"));
    QCOMPARE(result.value().choices[0].name, std::u16string(u"Writing Lab"));
    QCOMPARE(result.value().choices[0].grade, std::u16string(u"M2"));
    QCOMPARE(result.value().choices[0].level,
             std::u16string(u"Mixed (High)"));
    QCOMPARE(result.value().choices[0].room, std::u16string(u"Library"));
    QCOMPARE(result.value().choices[1].classId.value(), std::string("7"));
    QCOMPARE(result.value().choices[1].name, std::u16string(u"Oral Review"));
    QCOMPARE(result.value().choices[1].grade, std::u16string(u"E5"));
    QCOMPARE(result.value().choices[1].level, std::u16string(u"Lower"));
    QCOMPARE(result.value().choices[1].room, std::u16string(u"Room 204"));
}

void NextApplicationScheduleTestingClassChoicesReadQueryTests::
preservesAnEmptySuccessfulSnapshot()
{
    RecordingScheduleTestingClassChoicesReadPort port;
    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value().choices.empty());
}

void NextApplicationScheduleTestingClassChoicesReadQueryTests::
propagatesStructuredReadFailures()
{
    RecordingScheduleTestingClassChoicesReadPort port;
    const Domain::OperationError readError{
        .code = Domain::ErrorCode::Technical,
        .message = "testing class choices repository failed",
        .recoverable = false
    };
    port.result =
        Application::ScheduleTestingClassChoicesReadResult::failure(
            readError
            );

    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(!result);
    QVERIFY(result.error() == readError);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationScheduleTestingClassChoicesReadQueryTests)

#include "next_application_schedule_testing_class_choices_read_query_tests.moc"
