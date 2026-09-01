#pragma once

#include "core/result.h"
#include "domain/models/testing_class.h"

#include <QList>
#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class TestingClassRepository
{
public:
    explicit TestingClassRepository(
        QSqlDatabase& database
        );
    ~TestingClassRepository();

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
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        ) const;

    QSqlDatabase& m_database;
    mutable std::unique_ptr<classmngr::engine::SqliteDatabase>
        m_engineDatabase;
    mutable QString m_engineDatabasePath;
};
