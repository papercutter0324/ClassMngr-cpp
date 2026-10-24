#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/testing_class.h"

#include <QList>
#include <QSqlDatabase>


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
    explicit TestingClassRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryResult<int> createTestingClass(
        const TestingClass& testingClass,
        const QString& assignmentDay = {},
        const QString& assignmentStartTime = {}
        );

    [[nodiscard]] RepositoryStatus updateTestingClass(
        const TestingClass& testingClass
        );

    [[nodiscard]] RepositoryStatus updateTestingClassDetailed(
        const TestingClass& testingClass
        );

    [[nodiscard]] RepositoryResult<TestingClass> loadTestingClass(
        int classId
        );

    [[nodiscard]] RepositoryResult<TestingClassDetailsReadRecord>
        loadTestingClassDetailsReadRecord(int classId);

    [[nodiscard]] RepositoryResult<TestingClassDetailsReadRecord>
        loadTestingClassDetailsReadRecordDetailed(int classId);
    [[nodiscard]] const TestingClassDetailsReadMetrics&
        testingClassDetailsReadMetrics() const noexcept;

    [[nodiscard]] RepositoryResult<QList<TestingClass>> loadTestingClasses();

    [[nodiscard]] RepositoryResult<QList<TestingClassChoiceRecord>>
        loadTestingClassChoices();
    [[nodiscard]] const TestingClassChoicesReadMetrics&
        testingClassChoicesReadMetrics() const noexcept;

    [[nodiscard]] RepositoryStatus deleteTestingClass(
        int classId
        );

    [[nodiscard]] RepositoryStatus deleteTestingClassDetailed(
        int classId
        );

    [[nodiscard]] RepositoryResult<bool> isTestingClass(
        int classId
        );

private:
    [[nodiscard]] RepositoryStatus writeTestingClass(
        const TestingClass& testingClass
        );

    QSqlDatabase& m_database;
    TestingClassChoicesReadMetrics m_testingClassChoicesReadMetrics;
    TestingClassDetailsReadMetrics m_testingClassDetailsReadMetrics;
};
