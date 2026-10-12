#include "next/application/class_by_id_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(std::string value)
{
    return *Domain::ClassId::fromString(std::move(value));
}

class RecordingReadPort final : public ClassByIdReadPort
{
public:
    [[nodiscard]] ClassByIdReadResult readClassById(
        const Domain::ClassId& id
        ) const override
    {
        ++callCount;
        requests.push_back(id);
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requests;
    ClassByIdReadResult result = ClassByIdReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Unconfigured class read",
        .recoverable = false
    });
};

}

class NextApplicationClassByIdReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void forwardsCanonicalIdAndSnapshot();
    void rejectsNonCanonicalIdsBeforePort();
    void propagatesPortErrorAndRejectsMismatchedIdentity();
};

void NextApplicationClassByIdReadQueryTests::
forwardsCanonicalIdAndSnapshot()
{
    RecordingReadPort port;
    const Domain::ClassId requested = classId("42");
    const ClassByIdReadSnapshot expected{
        .classId = requested,
        .className = u"Orion"
    };
    port.result = ClassByIdReadResult::success(expected);
    const ClassByIdReadQuery query(port);

    const ClassByIdReadResult result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requests.front() == requested);
    QVERIFY(result.value() == expected);
}

void NextApplicationClassByIdReadQueryTests::
rejectsNonCanonicalIdsBeforePort()
{
    for (const std::string value : {"0", "042", "-1", "42x"})
    {
        RecordingReadPort port;
        const ClassByIdReadQuery query(port);

        const ClassByIdReadResult result = query.execute(classId(value));

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("Class ID must be a canonical positive integer.")
            );
        QVERIFY(result.error().recoverable);
        QCOMPARE(port.callCount, 0);
    }
}

void NextApplicationClassByIdReadQueryTests::
propagatesPortErrorAndRejectsMismatchedIdentity()
{
    RecordingReadPort port;
    const Domain::ClassId requested = classId("7");
    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "class read failed",
        .recoverable = true
    };
    port.result = ClassByIdReadResult::failure(expectedError);
    const ClassByIdReadQuery query(port);

    ClassByIdReadResult result = query.execute(requested);

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requests.front() == requested);

    port.result = ClassByIdReadResult::success({
        .classId = classId("8"),
        .className = u"Different class"
    });
    result = query.execute(requested);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        result.error().message,
        std::string("Class read returned a different class identifier.")
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 2);
}

QTEST_APPLESS_MAIN(NextApplicationClassByIdReadQueryTests)

#include "next_application_class_by_id_read_query_tests.moc"
