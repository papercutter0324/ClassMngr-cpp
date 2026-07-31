#pragma once

#include "core/result.h"
#include "domain/models/testing_class.h"

#include <QList>
#include <QSqlDatabase>

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

    [[nodiscard]] Status deleteTestingClass(
        int classId
        );

    [[nodiscard]] Result<bool> isTestingClass(
        int classId
        );

private:
    QSqlDatabase& m_database;
};
