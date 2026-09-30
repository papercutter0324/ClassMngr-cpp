#include "next/application/gs_team_directory_read_query.h"
#include "next/domain/domain_types.h"
#include "next/domain/native_english_teacher_id.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

static_assert(!std::is_same_v<Domain::GsTeamMemberId, Domain::TeacherId>);
static_assert(!std::is_convertible_v<
    Domain::GsTeamMemberId,
    Domain::TeacherId>);
static_assert(!std::is_same_v<
    Domain::GsTeamMemberId,
    Domain::NativeEnglishTeacherId>);
static_assert(!std::is_convertible_v<
    Domain::GsTeamMemberId,
    Domain::NativeEnglishTeacherId>);

GsTeamDirectoryEntry member(
    const int id,
    const std::u16string& name,
    const std::u16string& koreanName
    )
{
    return {
        .id = Domain::GsTeamMemberId(id),
        .name = name,
        .koreanName = koreanName,
        .position = u"Branch Manager",
        .phoneNumber = u"010-1234-5678",
        .birthday = u"03-14"
    };
}

class RecordingReadPort final : public GsTeamDirectoryReadPort
{
public:
    [[nodiscard]] GsTeamDirectoryReadResult
    readGsTeamDirectory() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    GsTeamDirectoryReadResult result = GsTeamDirectoryReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Unconfigured GS Team directory read",
        .recoverable = false
    });
};

}

class NextApplicationGsTeamDirectoryReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void callsPortOnceAndPreservesRowsAndFields();
    void propagatesPortError();
};

void NextApplicationGsTeamDirectoryReadQueryTests::
callsPortOnceAndPreservesRowsAndFields()
{
    RecordingReadPort port;
    const GsTeamDirectorySnapshot expected{
        member(8, u"Alex", u"\uC54C\uB809\uC2A4"),
        member(3, u"", u"\uAE40\uD558\uB298")
    };
    port.result = GsTeamDirectoryReadResult::success(expected);
    const GsTeamDirectoryReadQuery query(port);

    const GsTeamDirectoryReadResult result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().at(0).id.value(), 8);
    QCOMPARE(result.value().at(0).name, std::u16string(u"Alex"));
    QCOMPARE(result.value().at(0).koreanName,
        std::u16string(u"\uC54C\uB809\uC2A4"));
    QCOMPARE(result.value().at(0).position,
        std::u16string(u"Branch Manager"));
    QCOMPARE(result.value().at(0).phoneNumber,
        std::u16string(u"010-1234-5678"));
    QCOMPARE(result.value().at(0).birthday, std::u16string(u"03-14"));
    QCOMPARE(result.value().at(1).id.value(), 3);
}

void NextApplicationGsTeamDirectoryReadQueryTests::propagatesPortError()
{
    RecordingReadPort port;
    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "GS Team directory read failed",
        .recoverable = true
    };
    port.result = GsTeamDirectoryReadResult::failure(expectedError);
    const GsTeamDirectoryReadQuery query(port);

    const GsTeamDirectoryReadResult result = query.execute();

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationGsTeamDirectoryReadQueryTests)

#include "next_application_gs_team_directory_read_query_tests.moc"
