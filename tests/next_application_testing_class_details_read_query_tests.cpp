#include "next/application/testing_class_details_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

class RecordingTestingClassDetailsReadPort final
    : public Application::TestingClassDetailsReadPort
{
public:
    [[nodiscard]] Application::TestingClassDetailsReadResult
    readTestingClassDetails(
        const Application::TestingClassDetailsReadQuery& query
        ) const override
    {
        ++callCount;
        lastQuery = query;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TestingClassDetailsReadQuery>
        lastQuery;
    Application::TestingClassDetailsReadResult result =
        Application::TestingClassDetailsReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured testing class details read port.",
            .recoverable = false
        });
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

class NextApplicationTestingClassDetailsReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void forwardsTypedQueryOnceAndPreservesEverySnapshotField();
    void propagatesStructuredReadFailureOnce();
    void rejectsSnapshotForADifferentClass();
};

void NextApplicationTestingClassDetailsReadQueryTests::
forwardsTypedQueryOnceAndPreservesEverySnapshotField()
{
    RecordingTestingClassDetailsReadPort port;
    const Application::TestingClassDetailsReadQuery query{
        .classId = classId("class-42")
    };
    const Application::TestingClassDetailsSnapshot expected{
        .classId = classId("class-42"),
        .name = u"Writing Lab \uC2E4\uD5D8",
        .grade = u"M2",
        .level = u"Mixed (High)",
        .room = u"Library 204",
        .teacherId = teacherId("teacher-17"),
        .classColor = u"#123456",
        .fontColor = u"#FEDCBA",
        .notes = u"Keep the original notes \uB4F1\uB85D"
    };
    port.result =
        Application::TestingClassDetailsReadResult::success(expected);

    const auto result =
        Application::TestingClassDetailsReadQueryHandler::execute(
            query,
            port
            );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastQuery.has_value());
    QVERIFY(port.lastQuery.value() == query);
    QCOMPARE(port.lastQuery->classId, classId("class-42"));
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().classId, classId("class-42"));
    QCOMPARE(result.value().name, std::u16string(u"Writing Lab \uC2E4\uD5D8"));
    QCOMPARE(result.value().grade, std::u16string(u"M2"));
    QCOMPARE(result.value().level, std::u16string(u"Mixed (High)"));
    QCOMPARE(result.value().room, std::u16string(u"Library 204"));
    QVERIFY(result.value().teacherId.has_value());
    QCOMPARE(result.value().teacherId.value(), teacherId("teacher-17"));
    QCOMPARE(result.value().classColor, std::u16string(u"#123456"));
    QCOMPARE(result.value().fontColor, std::u16string(u"#FEDCBA"));
    QCOMPARE(
        result.value().notes,
        std::u16string(u"Keep the original notes \uB4F1\uB85D")
        );
}

void NextApplicationTestingClassDetailsReadQueryTests::
propagatesStructuredReadFailureOnce()
{
    RecordingTestingClassDetailsReadPort port;
    const Application::TestingClassDetailsReadQuery query{
        .classId = classId("42")
    };
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "testing class details repository failed",
        .recoverable = true
    };
    port.result =
        Application::TestingClassDetailsReadResult::failure(expected);

    const auto result =
        Application::TestingClassDetailsReadQueryHandler::execute(
            query,
            port
            );

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastQuery.has_value());
    QVERIFY(port.lastQuery.value() == query);
}

void NextApplicationTestingClassDetailsReadQueryTests::
rejectsSnapshotForADifferentClass()
{
    RecordingTestingClassDetailsReadPort port;
    const Application::TestingClassDetailsReadQuery query{
        .classId = classId("42")
    };
    port.result = Application::TestingClassDetailsReadResult::success({
        .classId = classId("43"),
        .name = u"Unexpected class"
    });

    const auto result =
        Application::TestingClassDetailsReadQueryHandler::execute(
            query,
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastQuery.has_value());
    QVERIFY(port.lastQuery.value() == query);
}

QTEST_APPLESS_MAIN(NextApplicationTestingClassDetailsReadQueryTests)

#include "next_application_testing_class_details_read_query_tests.moc"
