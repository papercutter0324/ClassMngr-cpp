#include "next/application/roster_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Application::RosterReadQuery query(std::string classId)
{
    const auto typedId = Domain::ClassId::fromString(std::move(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {.classId = *typedId};
}

Application::RosterSnapshot rawSnapshot()
{
    Application::RosterSnapshot snapshot;
    snapshot.columns = {
        u"Korean",
        u"Winter",
        u"English",
        u"Autumn",
        u"備考"
    };
    snapshot.columnWidths = {321, 222, 210, 333, 177};
    snapshot.rows.resize(38);
    for (std::vector<std::u16string>& row : snapshot.rows)
    {
        row.resize(5);
    }
    snapshot.rows[0] = {
        u"\uAE40\uBBFC\uC9C0",
        u"A+",
        u"First",
        u"Fall A",
        u"Review \U0001F4DA"
    };
    snapshot.rows[24][2] = u"Middle 25";
    snapshot.rows[37][4] = u"\uC218\uC5C5 \U0001F4DA";
    return snapshot;
}

class RecordingRosterReadPort final : public Application::RosterReadPort
{
public:
    [[nodiscard]] Application::RosterReadResult readRoster(
        const Application::RosterReadQuery& value
        ) const override
    {
        ++callCount;
        lastQuery = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::RosterReadQuery> lastQuery;
    Application::RosterReadResult result =
        Application::RosterReadResult::success({});
};

}

class NextApplicationRosterReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void invalidClassIdsDoNotReachPort();
    void preservesCompleteOrderedRawSnapshotAndUtf16Values();
    void preservesSuccessfulEmptyRoster();
    void preservesPortError();
};

void NextApplicationRosterReadQueryTests::invalidClassIdsDoNotReachPort()
{
    RecordingRosterReadPort port;
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
        const auto result = Application::RosterReadUseCase::execute(
            query(classId),
            port
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastQuery.has_value());
}

void NextApplicationRosterReadQueryTests::
preservesCompleteOrderedRawSnapshotAndUtf16Values()
{
    RecordingRosterReadPort port;
    const Application::RosterReadQuery request = query("42");
    const Application::RosterSnapshot expected = rawSnapshot();
    port.result = Application::RosterReadResult::success(expected);

    const auto result = Application::RosterReadUseCase::execute(request, port);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastQuery.value() == request);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().columns.size(), std::size_t(5));
    QCOMPARE(result.value().columns[0], std::u16string(u"Korean"));
    QCOMPARE(result.value().columns[3], std::u16string(u"Autumn"));
    QCOMPARE(result.value().columns[4], std::u16string(u"\u5099\u8003"));
    QCOMPARE(result.value().columnWidths, std::vector<int>({321, 222, 210, 333, 177}));
    QCOMPARE(result.value().rows.size(), std::size_t(38));
    QCOMPARE(result.value().rows[0].size(), std::size_t(5));
    QCOMPARE(result.value().rows[0][0], std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(result.value().rows[0][4], std::u16string(u"Review \U0001F4DA"));
    QCOMPARE(result.value().rows[24][2], std::u16string(u"Middle 25"));
    QVERIFY(result.value().rows[36][0].empty());
    QCOMPARE(result.value().rows[37][4], std::u16string(u"\uC218\uC5C5 \U0001F4DA"));
}

void NextApplicationRosterReadQueryTests::preservesSuccessfulEmptyRoster()
{
    RecordingRosterReadPort port;
    const Application::RosterReadQuery request = query("42");
    port.result = Application::RosterReadResult::success({});

    const auto result = Application::RosterReadUseCase::execute(request, port);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value().columns.empty());
    QVERIFY(result.value().columnWidths.empty());
    QVERIFY(result.value().rows.empty());
}

void NextApplicationRosterReadQueryTests::preservesPortError()
{
    RecordingRosterReadPort port;
    const Domain::OperationError error{
        .code = Domain::ErrorCode::Technical,
        .message = "roster read failed",
        .recoverable = true
    };
    port.result = Application::RosterReadResult::failure(error);

    const auto result = Application::RosterReadUseCase::execute(
        query("42"),
        port
        );

    QVERIFY(!result);
    QVERIFY(result.error() == error);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationRosterReadQueryTests)

#include "next_application_roster_read_query_tests.moc"
