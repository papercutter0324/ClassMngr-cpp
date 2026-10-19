#include "next/application/teacher_import_latest_source_date_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

static_assert(std::is_same_v<
    TeacherImportLatestSourceDateReadResult,
    ClassMngr::Next::Domain::Result<std::optional<
        ClassMngr::Next::Domain::GregorianDate>>>);

namespace
{

class RecordingReadPort final : public TeacherImportLatestSourceDateReadPort
{
public:
    [[nodiscard]] TeacherImportLatestSourceDateReadPortResult
        readLatestTeacherImportSourceDate() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    TeacherImportLatestSourceDateReadPortResult result =
        TeacherImportLatestSourceDateReadPortResult::success(std::nullopt);
};

}

class NextApplicationTeacherImportLatestSourceDateReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void returnsOptionalCanonicalDateAndReadsPortOnce();
    void preservesAbsentDateAndReadFailure();
    void rejectsMalformedAndImpossibleDates();
};

void NextApplicationTeacherImportLatestSourceDateReadQueryTests::
returnsOptionalCanonicalDateAndReadsPortOnce()
{
    RecordingReadPort port;
    port.result = TeacherImportLatestSourceDateReadPortResult::success(
        std::string("2026-09-01"));
    const TeacherImportLatestSourceDateReadQuery query(port);

    const TeacherImportLatestSourceDateReadResult result = query.execute();

    QVERIFY(result);
    QVERIFY(result.value().has_value());
    QCOMPARE(result.value()->toIsoString(), std::string("2026-09-01"));
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
    port.result = TeacherImportLatestSourceDateReadPortResult::failure(expectedError);
    result = query.execute();

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(port.callCount, 2);
}

void NextApplicationTeacherImportLatestSourceDateReadQueryTests::
rejectsMalformedAndImpossibleDates()
{
    RecordingReadPort port;
    const TeacherImportLatestSourceDateReadQuery query(port);

    const std::string invalidDates[] = {
        "2026-2-01",
        "2026-02-30",
        "2026-09-1",
        "2026/09/01"
    };
    int expectedCallCount = 0;
    for (const std::string& invalidDate : invalidDates)
    {
        port.result = TeacherImportLatestSourceDateReadPortResult::success(
            invalidDate);

        const TeacherImportLatestSourceDateReadResult result = query.execute();

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QCOMPARE(result.error().message,
            std::string("Teacher import source date must be a valid Gregorian ISO date."));
        QVERIFY(!result.error().recoverable);
        QCOMPARE(port.callCount, ++expectedCallCount);
    }
}

QTEST_APPLESS_MAIN(NextApplicationTeacherImportLatestSourceDateReadQueryTests)

#include "next_application_teacher_import_latest_source_date_read_query_tests.moc"
