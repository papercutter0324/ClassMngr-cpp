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

    [[nodiscard]] Result<QList<TestingBlock>> loadTestingBlocks();

    [[nodiscard]] Status saveTestingBlock(
        const QString& day,
        const QString& startTime,
        const QString& room
        );

    [[nodiscard]] Status deleteTestingBlock(
        const QString& day,
        const QString& startTime
        );

    [[nodiscard]] Status clearTestingBlocks();

private:
    QSqlDatabase& m_database;
};
