#include "next/application/gs_team_birthday_directory_read_query.h"

#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class RecordingPort final : public GsTeamBirthdayDirectoryReadPort
{
public:
    [[nodiscard]] GsTeamBirthdayDirectoryReadResult
    readGsTeamBirthdayDirectory() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    GsTeamBirthdayDirectoryReadResult result =
        GsTeamBirthdayDirectoryReadResult::success({});
};

}

class NextApplicationGsTeamBirthdayDirectoryReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void preservesRawRowsAndOrder();
    void preservesSuccessfulEmptyDirectory();
    void propagatesTypedFailure();
};

void NextApplicationGsTeamBirthdayDirectoryReadQueryTests::
preservesRawRowsAndOrder()
{
    RecordingPort port;
    port.result = GsTeamBirthdayDirectoryReadResult::success({
        {u"", u" 김하늘 ", u" M1 ", u" 04-15 "},
        {u" Alex ", u" 알렉스 ", u" Branch Manager ", u"01-02"}
    });
    const GsTeamBirthdayDirectoryReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value()[0].name, std::u16string());
    QCOMPARE(result.value()[0].koreanName, std::u16string(u" 김하늘 "));
    QCOMPARE(result.value()[0].position, std::u16string(u" M1 "));
    QCOMPARE(result.value()[0].birthday, std::u16string(u" 04-15 "));
    QCOMPARE(result.value()[1].name, std::u16string(u" Alex "));
}

void NextApplicationGsTeamBirthdayDirectoryReadQueryTests::
preservesSuccessfulEmptyDirectory()
{
    RecordingPort port;
    port.result = GsTeamBirthdayDirectoryReadResult::success({});
    const GsTeamBirthdayDirectoryReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 1);
}

void NextApplicationGsTeamBirthdayDirectoryReadQueryTests::
propagatesTypedFailure()
{
    RecordingPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "GS Team birthday read failed",
        .recoverable = false
    };
    port.result = GsTeamBirthdayDirectoryReadResult::failure(expected);
    const GsTeamBirthdayDirectoryReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationGsTeamBirthdayDirectoryReadQueryTests)

#include "next_application_gs_team_birthday_directory_read_query_tests.moc"
