#pragma once

#include "core/result.h"
#include "domain/models/testing_block.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class TestingBlockRepository
{
public:
    explicit TestingBlockRepository(
        QSqlDatabase& database
        );
    ~TestingBlockRepository();

    [[nodiscard]] Result<QList<TestingAssignment>>
    loadTestingAssignments();

    [[nodiscard]] Result<QList<TestingBlock>> loadTestingBlocks();

    [[nodiscard]] Status saveTestingBlock(
        const QString& day,
        const QString& startTime,
        const QString& room,
        bool replaceExisting = false
        );

    [[nodiscard]] Status assignTestingClass(
        const QString& day,
        const QString& startTime,
        int classId,
        bool replaceExisting = false
        );

    [[nodiscard]] Status deleteTestingAssignment(
        const QString& day,
        const QString& startTime
        );

    [[nodiscard]] Status deleteTestingBlock(
        const QString& day,
        const QString& startTime
        );

    [[nodiscard]] Status clearTestingAssignments();

    [[nodiscard]] Status clearTestingBlocks();

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        ) const;

    QSqlDatabase& m_database;
    mutable std::unique_ptr<classmngr::engine::SqliteDatabase>
        m_engineDatabase;
    mutable QString m_engineDatabasePath;
};
