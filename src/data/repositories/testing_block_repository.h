#pragma once

#include "core/result.h"
#include "domain/models/testing_block.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

class TestingBlockRepository
{
public:
    explicit TestingBlockRepository(
        QSqlDatabase& database
        );

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
    QSqlDatabase& m_database;
};
