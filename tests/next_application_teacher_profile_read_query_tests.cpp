#include "next/application/teacher_profile_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::TeacherId teacherId(const int value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

Domain::TeacherProfileFields fields()
{
    return {
        .teacherKr = u"김선생",
        .teacherEn = u"Alex Teacher",
        .preferredRomanization = u"Seon Kim",
        .preferredName = u"Alex",
        .roomNumber = u"Room 5",
        .birthday = u"03-14",
        .phoneNumber = u"010-1234-5678",
        .wifiName = u"Class Wi-Fi",
        .wifiPassword = u"wifi secret",
        .internetType = u"Both",
        .zoomId = u"alex.zoom",
        .zoomPassword = u"zoom secret",
        .projectionType = u"Zoom",
        .notes = u"Teacher profile notes"
    };
}

class RecordingReadPort final : public TeacherProfileReadPort
{
public:
    [[nodiscard]] TeacherProfileReadResult readTeacherProfile(
        const Domain::TeacherId& id
        ) const override
    {
        ++callCount;
        requests.push_back(id);
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::TeacherId> requests;
    TeacherProfileReadResult result = TeacherProfileReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Unconfigured teacher profile read",
        .recoverable = false
    });
};

}

class NextApplicationTeacherProfileReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void callsPortOnceWithTypedIdAndPreservesAllFields();
    void propagatesPortErrorAndRejectsMismatchedIdentity();
};

void NextApplicationTeacherProfileReadQueryTests::
callsPortOnceWithTypedIdAndPreservesAllFields()
{
    RecordingReadPort port;
    const Domain::TeacherId requested = teacherId(42);
    const TeacherProfileReadSnapshot expected{
        .teacherId = requested,
        .fields = fields()
    };
    port.result = TeacherProfileReadResult::success(expected);
    const TeacherProfileReadQuery query(port);

    const TeacherProfileReadResult result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requests.front() == requested);
    QVERIFY(result.value() == expected);
}

void NextApplicationTeacherProfileReadQueryTests::
propagatesPortErrorAndRejectsMismatchedIdentity()
{
    RecordingReadPort port;
    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "teacher profile read failed",
        .recoverable = true
    };
    port.result = TeacherProfileReadResult::failure(expectedError);
    const TeacherProfileReadQuery query(port);

    TeacherProfileReadResult result = query.execute(teacherId(7));

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requests.back() == teacherId(7));

    port.result = TeacherProfileReadResult::success({
        .teacherId = teacherId(8),
        .fields = fields()
    });
    result = query.execute(teacherId(7));
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 2);

    const auto nonCanonicalId = Domain::TeacherId::fromString("07");
    QVERIFY(nonCanonicalId);
    result = query.execute(*nonCanonicalId);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(port.callCount, 2);
}

QTEST_APPLESS_MAIN(NextApplicationTeacherProfileReadQueryTests)

#include "next_application_teacher_profile_read_query_tests.moc"
