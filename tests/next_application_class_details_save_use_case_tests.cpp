#include "next/application/class_details_save_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

Application::ClassDetailsSaveRequest request(std::string classId)
{
    return {
        .classId = *Domain::ClassId::fromString(classId),
        .classGrade = u"E4",
        .classLevel = u"Theseus",
        .readingBook = u"Reading Explorer 1",
        .essayBook = u"",
        .classColor = u"#123456",
        .fontColor = u"#654321",
        .regularTimes = {
            *Domain::ScheduleTime::fromMinutes(0, 9 * 60, 9 * 60 + 50)
        },
        .intensiveTimes = {
            *Domain::ScheduleTime::fromMinutes(4, 10 * 60, 10 * 60 + 50)
        }
    };
}

class RecordingSavePort final : public Application::ClassDetailsSavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveClassDetails(
        const Application::ClassDetailsSaveRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassDetailsSaveRequest> lastRequest;
    Domain::Result<void> result = Domain::Result<void>::success();
};

}

class NextApplicationClassDetailsSaveUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void invalidClassIdsDoNotReachPort();
    void validRequestPreservesTypedScheduleValues();
    void preservesPortFailure();
};

void NextApplicationClassDetailsSaveUseCaseTests::
invalidClassIdsDoNotReachPort()
{
    RecordingSavePort port;
    for (const std::string classId : {"0", "-2", "class-42"})
    {
        const auto result = Application::ClassDetailsSaveUseCase::execute(
            request(classId),
            port
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationClassDetailsSaveUseCaseTests::
validRequestPreservesTypedScheduleValues()
{
    RecordingSavePort port;
    const Application::ClassDetailsSaveRequest value = request("42");

    const auto result =
        Application::ClassDetailsSaveUseCase::execute(value, port);
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId.value(), std::string("42"));
    QCOMPARE(port.lastRequest->classGrade, std::u16string(u"E4"));
    QCOMPARE(port.lastRequest->regularTimes, value.regularTimes);
    QCOMPARE(port.lastRequest->intensiveTimes, value.intensiveTimes);
}

void NextApplicationClassDetailsSaveUseCaseTests::preservesPortFailure()
{
    RecordingSavePort port;
    port.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "save rejected",
        .recoverable = true
    });

    const auto result = Application::ClassDetailsSaveUseCase::execute(
        request("42"),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("save rejected"));
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationClassDetailsSaveUseCaseTests)

#include "next_application_class_details_save_use_case_tests.moc"
