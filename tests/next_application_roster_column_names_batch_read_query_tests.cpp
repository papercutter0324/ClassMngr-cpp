#include "next/application/roster_column_names_batch_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

class RecordingRosterColumnNamesBatchReadPort final
    : public RosterColumnNamesBatchReadPort
{
public:
    [[nodiscard]] RosterColumnNamesBatchReadResult readRosterColumnNames(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        ++callCount;
        lastClassIds = classIds;
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> lastClassIds;
    RosterColumnNamesBatchReadResult result =
        RosterColumnNamesBatchReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured roster column names read",
            .recoverable = false
        });
};

}

class NextApplicationRosterColumnNamesBatchReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void returnsOrderedClassColumnNamesAndSuccessfulEmptySlots();
    void emptyRequestDoesNotCallPort();
    void rejectsInvalidAndDuplicateIdsBeforePortCall();
    void preservesStructuredPortFailure();
    void rejectsMismatchedResultCountAndIdentifierOrder();
};

void NextApplicationRosterColumnNamesBatchReadQueryTests::
returnsOrderedClassColumnNamesAndSuccessfulEmptySlots()
{
    RecordingRosterColumnNamesBatchReadPort port;
    const std::vector<Domain::ClassId> requestedIds{
        classId("42"),
        classId("7"),
        classId("108")
    };
    const std::vector<RosterColumnNamesReadSnapshot> expected{
        {classId("42"), {u"First", u"Second"}},
        {classId("7"), {}},
        {classId("108"), {u"Korean", u"Memo"}}
    };
    port.result = RosterColumnNamesBatchReadResult::success(expected);
    const RosterColumnNamesBatchReadQuery query(port);

    const auto result = query.execute(requestedIds);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastClassIds == requestedIds);
    QVERIFY(result.value() == expected);
}

void NextApplicationRosterColumnNamesBatchReadQueryTests::
emptyRequestDoesNotCallPort()
{
    RecordingRosterColumnNamesBatchReadPort port;
    const RosterColumnNamesBatchReadQuery query(port);

    const auto result = query.execute({});

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 0);
}

void NextApplicationRosterColumnNamesBatchReadQueryTests::
rejectsInvalidAndDuplicateIdsBeforePortCall()
{
    const std::vector<std::vector<std::string>> invalidRows{
        {"0"},
        {"01"},
        {"-1"},
        {"2147483648"},
        {"42", "42"}
    };

    for (const auto& ids : invalidRows)
    {
        RecordingRosterColumnNamesBatchReadPort port;
        std::vector<Domain::ClassId> requestedIds;
        for (const std::string& id : ids)
        {
            requestedIds.push_back(classId(id));
        }
        const RosterColumnNamesBatchReadQuery query(port);

        const auto result = query.execute(requestedIds);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(port.callCount, 0);
    }
}

void NextApplicationRosterColumnNamesBatchReadQueryTests::
preservesStructuredPortFailure()
{
    RecordingRosterColumnNamesBatchReadPort port;
    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "roster column statement failed",
        .recoverable = false
    };
    port.result = RosterColumnNamesBatchReadResult::failure(expectedError);
    const RosterColumnNamesBatchReadQuery query(port);

    const auto result = query.execute({classId("42"), classId("7")});

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(port.callCount, 1);
}

void NextApplicationRosterColumnNamesBatchReadQueryTests::
rejectsMismatchedResultCountAndIdentifierOrder()
{
    const std::vector<Domain::ClassId> requestedIds{
        classId("42"),
        classId("7")
    };

    RecordingRosterColumnNamesBatchReadPort shortPort;
    shortPort.result = RosterColumnNamesBatchReadResult::success({
        {classId("42"), {u"Notes"}}
    });
    const RosterColumnNamesBatchReadQuery shortQuery(shortPort);
    const auto shortResult = shortQuery.execute(requestedIds);
    QVERIFY(!shortResult);
    QCOMPARE(shortResult.error().code, Domain::ErrorCode::Validation);

    RecordingRosterColumnNamesBatchReadPort reorderedPort;
    reorderedPort.result = RosterColumnNamesBatchReadResult::success({
        {classId("7"), {u"Notes"}},
        {classId("42"), {u"Other"}}
    });
    const RosterColumnNamesBatchReadQuery reorderedQuery(reorderedPort);
    const auto reorderedResult = reorderedQuery.execute(requestedIds);
    QVERIFY(!reorderedResult);
    QCOMPARE(reorderedResult.error().code, Domain::ErrorCode::Validation);
}

QTEST_APPLESS_MAIN(NextApplicationRosterColumnNamesBatchReadQueryTests)

#include "next_application_roster_column_names_batch_read_query_tests.moc"
