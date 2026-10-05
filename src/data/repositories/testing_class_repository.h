#pragma once

#include "core/result.h"
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

struct TestingClassChoicesReadMetrics final
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

    [[nodiscard]] Result<int> createTestingClass(
        const TestingClass& testingClass,
        const QString& assignmentDay = {},
        const QString& assignmentStartTime = {}
        );

    [[nodiscard]] Status updateTestingClass(
        const TestingClass& testingClass
        );

    [[nodiscard]] Result<TestingClass> loadTestingClass(
        int classId
        );

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
    QSqlDatabase& m_database;
    TestingClassChoicesReadMetrics m_testingClassChoicesReadMetrics;
};
