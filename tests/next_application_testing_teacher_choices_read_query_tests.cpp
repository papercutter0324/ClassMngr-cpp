#include "next/application/testing_teacher_choices_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

class RecordingTestingTeacherChoicesReadPort final
    : public Application::TestingTeacherChoicesReadPort
{
public:
    [[nodiscard]] Application::TestingTeacherChoicesReadResult
    readTestingTeacherChoices(
        const Application::TestingTeacherChoicesReadQuery& query
        ) const override
    {
        ++callCount;
        lastQuery = query;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TestingTeacherChoicesReadQuery>
        lastQuery;
    Application::TestingTeacherChoicesReadResult result =
        Application::TestingTeacherChoicesReadResult::success({});
};

Domain::TeacherId teacherId(const std::string& value)
{
    const auto parsed = Domain::TeacherId::fromString(value);
    if (!parsed)
    {
        qFatal("Test teacher ID must have a typed representation.");
    }
    return *parsed;
}

}

class NextApplicationTestingTeacherChoicesReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void forwardsTypedQueryAndPreservesOrderedChoicesAndFields();
    void preservesAnEmptySuccessfulSnapshot();
    void propagatesStructuredReadFailures();
};

void NextApplicationTestingTeacherChoicesReadQueryTests::
forwardsTypedQueryAndPreservesOrderedChoicesAndFields()
{
    RecordingTestingTeacherChoicesReadPort port;
    const Application::TestingTeacherChoicesReadQuery query;
    const Application::TestingTeacherChoicesSnapshot expected{
        .choices = {
            {
                .teacherId = teacherId("teacher-17"),
                .name = u"  김선생  ",
                .room = u"  실험실 204  "
            },
            {
                .teacherId = teacherId("teacher-3"),
                .name = u"이선생",
                .room = u"Lab 2"
            }
        }
    };
    port.result =
        Application::TestingTeacherChoicesReadResult::success(expected);

    const auto result =
        Application::TestingTeacherChoicesReadQueryHandler::execute(
            query,
            port
            );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastQuery.has_value());
    QVERIFY(port.lastQuery.value() == query);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().choices.size(), std::size_t(2));
    QCOMPARE(
        result.value().choices[0].teacherId.value(),
        std::string("teacher-17")
        );
    QCOMPARE(
        result.value().choices[0].name,
        std::u16string(u"  김선생  ")
        );
    QCOMPARE(
        result.value().choices[0].room,
        std::u16string(u"  실험실 204  ")
        );
    QCOMPARE(
        result.value().choices[1].teacherId.value(),
        std::string("teacher-3")
        );
    QCOMPARE(result.value().choices[1].name, std::u16string(u"이선생"));
    QCOMPARE(result.value().choices[1].room, std::u16string(u"Lab 2"));
}

void NextApplicationTestingTeacherChoicesReadQueryTests::
preservesAnEmptySuccessfulSnapshot()
{
    RecordingTestingTeacherChoicesReadPort port;
    const auto result =
        Application::TestingTeacherChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value().choices.empty());
}

void NextApplicationTestingTeacherChoicesReadQueryTests::
propagatesStructuredReadFailures()
{
    RecordingTestingTeacherChoicesReadPort port;
    const Domain::OperationError readError{
        .code = Domain::ErrorCode::Technical,
        .message = "testing teacher choices repository failed",
        .recoverable = false
    };
    port.result =
        Application::TestingTeacherChoicesReadResult::failure(readError);

    const auto result =
        Application::TestingTeacherChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(!result);
    QVERIFY(result.error() == readError);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationTestingTeacherChoicesReadQueryTests)

#include "next_application_testing_teacher_choices_read_query_tests.moc"
