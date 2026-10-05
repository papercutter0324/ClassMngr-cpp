#include "next/application/teacher_display_name_batch_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::TeacherId teacherId(const std::string& value)
{
    return *Domain::TeacherId::fromString(value);
}

TeacherDisplayNameBatchReadSnapshot snapshot(
    const std::string& id,
    const std::u16string& displayName
    )
{
    return {
        .teacherId = teacherId(id),
        .fields = {
            .teacherKr = u"김선생",
            .teacherEn = u"Teacher",
            .preferredRomanization = u"Seon Kim",
            .preferredName = displayName
        }
    };
}

class RecordingReadPort final : public TeacherDisplayNameBatchReadPort
{
public:
    [[nodiscard]] TeacherDisplayNameBatchReadResult readTeacherDisplayNames(
        const std::vector<Domain::TeacherId>& teacherIds
        ) const override
    {
        ++callCount;
        requests = teacherIds;
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::TeacherId> requests;
    TeacherDisplayNameBatchReadResult result =
        TeacherDisplayNameBatchReadResult::success({});
};

}

class NextApplicationTeacherDisplayNameBatchReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void returnsPartialRowsInRequestedOrder();
    void rejectsInvalidAndDuplicateInputBeforeCallingPort();
    void rejectsUnexpectedDuplicateAndMisorderedOutputIdentity();
    void returnsEmptyResultWithoutCallingPort();
};

void NextApplicationTeacherDisplayNameBatchReadQueryTests::
returnsPartialRowsInRequestedOrder()
{
    RecordingReadPort port;
    const std::vector<Domain::TeacherId> requested{
        teacherId("4"), teacherId("2"), teacherId("3")
    };
    const std::vector<TeacherDisplayNameBatchReadSnapshot> expected{
        snapshot("4", u"Fourth"), snapshot("3", u"Third")
    };
    port.result = TeacherDisplayNameBatchReadResult::success(expected);
    const TeacherDisplayNameBatchReadQuery query(port);

    const TeacherDisplayNameBatchReadResult result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QCOMPARE(port.requests.size(), requested.size());
    QVERIFY(port.requests[0] == requested[0]);
    QVERIFY(port.requests[1] == requested[1]);
    QVERIFY(port.requests[2] == requested[2]);
    QCOMPARE(result.value().size(), expected.size());
    QVERIFY(result.value() == expected);
}

void NextApplicationTeacherDisplayNameBatchReadQueryTests::
rejectsInvalidAndDuplicateInputBeforeCallingPort()
{
    RecordingReadPort port;
    const TeacherDisplayNameBatchReadQuery query(port);

    for (const std::string& invalid : {
             std::string("0"), std::string("01"), std::string("-1"),
             std::string("2147483648")})
    {
        const auto result = query.execute({teacherId(invalid)});
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QCOMPARE(port.callCount, 0);

    const auto duplicate = query.execute({teacherId("7"), teacherId("7")});
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(port.callCount, 0);
}

void NextApplicationTeacherDisplayNameBatchReadQueryTests::
rejectsUnexpectedDuplicateAndMisorderedOutputIdentity()
{
    RecordingReadPort port;
    const std::vector<Domain::TeacherId> requested{
        teacherId("1"), teacherId("2")
    };
    const TeacherDisplayNameBatchReadQuery query(port);

    port.result = TeacherDisplayNameBatchReadResult::success({
        snapshot("01", u"Noncanonical")
    });
    auto result = query.execute(requested);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.result = TeacherDisplayNameBatchReadResult::success({
        snapshot("3", u"Unexpected")
    });
    result = query.execute(requested);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.result = TeacherDisplayNameBatchReadResult::success({
        snapshot("1", u"First"), snapshot("1", u"Duplicate")
    });
    result = query.execute(requested);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.result = TeacherDisplayNameBatchReadResult::success({
        snapshot("2", u"Second"), snapshot("1", u"First")
    });
    result = query.execute(requested);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(port.callCount, 4);
}

void NextApplicationTeacherDisplayNameBatchReadQueryTests::
returnsEmptyResultWithoutCallingPort()
{
    RecordingReadPort port;
    const TeacherDisplayNameBatchReadQuery query(port);

    const TeacherDisplayNameBatchReadResult result = query.execute({});

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 0);
}

QTEST_APPLESS_MAIN(NextApplicationTeacherDisplayNameBatchReadQueryTests)

#include "next_application_teacher_display_name_batch_read_query_tests.moc"
