#include "next/application/roster_save_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Application::RosterSaveRequest request(std::string classId)
{
    const auto typedId = Domain::ClassId::fromString(std::move(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    Application::RosterSnapshot snapshot;
    snapshot.columns = {u"English", u"Korean", u"Winter", u"備考"};
    snapshot.columnWidths = {171, 121, 130, 209};
    snapshot.rows.resize(25);
    snapshot.rows[0] = {u"Alice", u"김민지", u"A+", u"수업 \U0001F4DA"};
    snapshot.rows[3] = {u"Zoë", u"박지훈", u"B", u"점검"};
    snapshot.rows[24] = {u"Final Row", u"최민서", u"C", u"끝"};

    return {
        .classId = *typedId,
        .roster = std::move(snapshot),
        .allowQuestionableKoreanNameLengths = true
    };
}

class RecordingRosterSavePort final : public Application::RosterSavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveRoster(
        const Application::RosterSaveRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::RosterSaveRequest> lastRequest;
    Domain::Result<void> result = Domain::Result<void>::success();
};

}

class NextApplicationRosterSaveUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void invalidClassIdsDoNotReachPort();
    void preservesCompleteSnapshotAndAllowFlag();
    void preservesPortFailure();
};

void NextApplicationRosterSaveUseCaseTests::invalidClassIdsDoNotReachPort()
{
    RecordingRosterSavePort port;
    for (const std::string classId : {
             "0",
             "-2",
             "+42",
             "class-42",
             " 42",
             "42 ",
             "01",
             "00042",
             "999999999999999999999"
         })
    {
        const auto result = Application::RosterSaveUseCase::execute(
            request(classId),
            port
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationRosterSaveUseCaseTests::preservesCompleteSnapshotAndAllowFlag()
{
    RecordingRosterSavePort port;
    const Application::RosterSaveRequest value = request("42");

    const auto result = Application::RosterSaveUseCase::execute(value, port);
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(port.lastRequest.value() == value);
    QCOMPARE(port.lastRequest->classId.value(), std::string("42"));
    QCOMPARE(port.lastRequest->roster.columns.size(), std::size_t(4));
    QCOMPARE(port.lastRequest->roster.columnWidths,
        std::vector<int>({171, 121, 130, 209}));
    QCOMPARE(port.lastRequest->roster.rows.size(), std::size_t(25));
    QVERIFY(port.lastRequest->roster.rows[1].empty());
    QCOMPARE(port.lastRequest->roster.rows[24][0], std::u16string(u"Final Row"));
    QCOMPARE(port.lastRequest->allowQuestionableKoreanNameLengths, true);
}

void NextApplicationRosterSaveUseCaseTests::preservesPortFailure()
{
    RecordingRosterSavePort port;
    port.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "save rejected",
        .recoverable = true
    });

    const auto result = Application::RosterSaveUseCase::execute(
        request("42"),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("save rejected"));
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationRosterSaveUseCaseTests)

#include "next_application_roster_save_use_case_tests.moc"
