#pragma once

#include "core/result.h"
#include "domain/models/testing_class.h"

#include <QList>
#include <QSqlDatabase>

#include <expected>

struct TestingClassChoiceRecord final
{
    int classId = -1;
    QString name;
    QString grade;
    QString level;
    QString room;
};

struct TestingClassDetailsReadRecord final
{
    int classId = -1;
    QString name;
    QString grade;
    QString level;
    QString room;
    qint64 teacherId = -1;
    QString classColor;
    QString fontColor;
    QString notes;
};

struct TestingClassDetailsReadFailure final
{
    enum class Kind
    {
        ParentClassMissing,
        Technical
    };

    Kind kind = Kind::Technical;
    QString diagnostic;
};

using TestingClassDetailsReadResult =
    std::expected<
        TestingClassDetailsReadRecord,
        TestingClassDetailsReadFailure
        >;

struct TestingClassChoicesReadMetrics final
{
    int callCount = 0;
    int statementCount = 0;
};

struct TestingClassDetailsReadMetrics final
{
    int callCount = 0;
    int statementCount = 0;
};

class TestingClassRepository
{
public:
    struct UpdateFailure final
    {
        enum class Kind
        {
            ParentClassMissing,
            Technical
        };

        Kind kind = Kind::Technical;
        QString diagnostic;
    };

    using UpdateResult = std::expected<void, UpdateFailure>;

    explicit TestingClassRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] Result<int> createTestingClass(
        const TestingClass& testingClass,
        const QString& assignmentDay = {},
        const QString& assignmentStartTime = {}
        );

    [[nodiscard]] Status updateTestingClass(
        const TestingClass& testingClass
        );

    // The typed details-update path can distinguish a missing parent class
    // while preserving the legacy Status diagnostic and behavior.
    [[nodiscard]] UpdateResult updateTestingClassDetailed(
        const TestingClass& testingClass
        );

    [[nodiscard]] Result<TestingClass> loadTestingClass(
        int classId
        );

    [[nodiscard]] Result<TestingClassDetailsReadRecord>
        loadTestingClassDetailsReadRecord(int classId);

    // The typed details-read path can distinguish a missing parent class
    // while preserving the legacy Result diagnostic and behavior.
    [[nodiscard]] TestingClassDetailsReadResult
        loadTestingClassDetailsReadRecordDetailed(int classId);
    [[nodiscard]] const TestingClassDetailsReadMetrics&
        testingClassDetailsReadMetrics() const noexcept;

    [[nodiscard]] Result<QList<TestingClass>> loadTestingClasses();

    [[nodiscard]] Result<QList<TestingClassChoiceRecord>>
        loadTestingClassChoices();
    [[nodiscard]] const TestingClassChoicesReadMetrics&
        testingClassChoicesReadMetrics() const noexcept;

    [[nodiscard]] Status deleteTestingClass(
        int classId
        );

    [[nodiscard]] Result<bool> isTestingClass(
        int classId
        );

private:
    [[nodiscard]] Status writeTestingClass(
        const TestingClass& testingClass
        );

    QSqlDatabase& m_database;
    TestingClassChoicesReadMetrics m_testingClassChoicesReadMetrics;
    TestingClassDetailsReadMetrics m_testingClassDetailsReadMetrics;
};
