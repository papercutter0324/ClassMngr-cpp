#include "next/application/teacher_import_latest_source_date_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class RecordingReadPort final : public TeacherImportLatestSourceDateReadPort
{
public:
    [[nodiscard]] TeacherImportLatestSourceDateReadResult
        readLatestTeacherImportSourceDate() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    TeacherImportLatestSourceDateReadResult result =
        TeacherImportLatestSourceDateReadResult::success(std::nullopt);
};

}

class NextApplicationTeacherImportLatestSourceDateReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void returnsOptionalCanonicalDateAndReadsPortOnce();
    void preservesAbsentDateAndReadFailure();
};

void NextApplicationTeacherImportLatestSourceDateReadQueryTests::
returnsOptionalCanonicalDateAndReadsPortOnce()
{
    RecordingReadPort port;
    port.result = TeacherImportLatestSourceDateReadResult::success(
        std::string("2026-09-01"));
    const TeacherImportLatestSourceDateReadQuery query(port);

    const TeacherImportLatestSourceDateReadResult result = query.execute();

    QVERIFY(result);
    QVERIFY(result.value().has_value());
    QCOMPARE(*result.value(), std::string("2026-09-01"));
    QCOMPARE(port.callCount, 1);
}

void NextApplicationTeacherImportLatestSourceDateReadQueryTests::
preservesAbsentDateAndReadFailure()
{
    RecordingReadPort port;
    const TeacherImportLatestSourceDateReadQuery query(port);

    TeacherImportLatestSourceDateReadResult result = query.execute();

    QVERIFY(result);
    QVERIFY(!result.value().has_value());
    QCOMPARE(port.callCount, 1);

    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "settings read failed",
        .recoverable = false
    };
    port.result = TeacherImportLatestSourceDateReadResult::failure(expectedError);
    result = query.execute();

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(port.callCount, 2);
}

QTEST_APPLESS_MAIN(NextApplicationTeacherImportLatestSourceDateReadQueryTests)

#include "next_application_teacher_import_latest_source_date_read_query_tests.moc"
