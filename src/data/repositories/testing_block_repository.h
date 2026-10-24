#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/testing_block.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

struct TestingAssignmentDisplayRecord final
{
    QString day;
    QString startTime;
    QString room;
    int classId{-1};
    bool hasSpecialClass{false};
    QString className;
    QString teacherKoreanName;
    QString teacherEnglishName;
    QString teacherPreferredName;
    QString testingClassRoom;
    QString grade;
    QString level;
    QString classColor{QStringLiteral("#FFFFFF")};
    QString fontColor{QStringLiteral("#000000")};
};

struct TestingAssignmentDisplayReadMetrics final
{
    int callCount = 0;
    int statementCount = 0;
};

class TestingBlockRepository
{
public:
    explicit TestingBlockRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryResult<QList<TestingAssignment>>
    loadTestingAssignments();

    [[nodiscard]] RepositoryResult<QList<TestingAssignmentDisplayRecord>>
    loadTestingAssignmentDisplayRecords();
    [[nodiscard]] const TestingAssignmentDisplayReadMetrics&
    testingAssignmentDisplayReadMetrics() const noexcept;

    [[nodiscard]] RepositoryResult<QList<TestingBlock>> loadTestingBlocks();

    [[nodiscard]] RepositoryStatus saveTestingBlock(
        const QString& day,
        const QString& startTime,
        const QString& room,
        bool replaceExisting = false
        );

    [[nodiscard]] RepositoryStatus assignTestingClass(
        const QString& day,
        const QString& startTime,
        int classId,
        bool replaceExisting = false
        );

    [[nodiscard]] RepositoryStatus deleteTestingAssignment(
        const QString& day,
        const QString& startTime
        );

    [[nodiscard]] RepositoryStatus deleteTestingBlock(
        const QString& day,
        const QString& startTime
        );

    [[nodiscard]] RepositoryStatus clearTestingAssignments();

    [[nodiscard]] RepositoryStatus clearTestingBlocks();

private:
    QSqlDatabase& m_database;
    TestingAssignmentDisplayReadMetrics
        m_testingAssignmentDisplayReadMetrics;
};
