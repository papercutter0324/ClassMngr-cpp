#include "next/application/class_teacher_assignments_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::TeacherId teacherId(const std::string& value)
{
    const auto parsed = Domain::TeacherId::fromString(value);
    if (!parsed)
    {
        qFatal("Test teacher ID must have a typed representation.");
    }
    return *parsed;
}

class RecordingClassTeacherAssignmentsReadPort final
    : public ClassTeacherAssignmentsReadPort
{
public:
    [[nodiscard]] ClassTeacherAssignmentsReadResult
    readClassTeacherAssignments() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    ClassTeacherAssignmentsReadResult result =
        ClassTeacherAssignmentsReadResult::success({});
};

}

class NextApplicationClassTeacherAssignmentsReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void preservesAssignmentRowOrderAndUnassignedRows();
    void treatsMalformedTeacherIdsAsUnassignedWithoutDroppingRows();
    void propagatesStructuredReadFailure();
};

void NextApplicationClassTeacherAssignmentsReadQueryTests::
preservesAssignmentRowOrderAndUnassignedRows()
{
    RecordingClassTeacherAssignmentsReadPort port;
    const ClassTeacherAssignmentsReadSnapshot expected{
        .assignments = {
            {.teacherId = teacherId("17")},
            {.teacherId = std::nullopt},
            {.teacherId = teacherId("3")}
        }
    };
    port.result = ClassTeacherAssignmentsReadResult::success(expected);
    const ClassTeacherAssignmentsReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().assignments.size(), std::size_t(3));
    QCOMPARE(
        result.value().assignments[0].teacherId->value(),
        std::string("17")
        );
    QVERIFY(!result.value().assignments[1].teacherId);
    QCOMPARE(
        result.value().assignments[2].teacherId->value(),
        std::string("3")
        );
}

void NextApplicationClassTeacherAssignmentsReadQueryTests::
treatsMalformedTeacherIdsAsUnassignedWithoutDroppingRows()
{
    RecordingClassTeacherAssignmentsReadPort port;
    port.result = ClassTeacherAssignmentsReadResult::success({
        .assignments = {
            {.teacherId = teacherId("0")},
            {.teacherId = teacherId("01")},
            {.teacherId = teacherId("-1")},
            {.teacherId = teacherId("2147483648")},
            {.teacherId = teacherId("23")}
        }
    });
    const ClassTeacherAssignmentsReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(result.value().assignments.size(), std::size_t(5));
    for (std::size_t index = 0; index < 4; ++index)
    {
        QVERIFY(!result.value().assignments[index].teacherId);
    }
    QVERIFY(result.value().assignments[4].teacherId);
    QCOMPARE(
        result.value().assignments[4].teacherId->value(),
        std::string("23")
        );
}

void NextApplicationClassTeacherAssignmentsReadQueryTests::
propagatesStructuredReadFailure()
{
    RecordingClassTeacherAssignmentsReadPort port;
    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "class teacher assignment read failed",
        .recoverable = false
    };
    port.result = ClassTeacherAssignmentsReadResult::failure(expectedError);
    const ClassTeacherAssignmentsReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(
    NextApplicationClassTeacherAssignmentsReadQueryTests
    )

#include "next_application_class_teacher_assignments_read_query_tests.moc"
