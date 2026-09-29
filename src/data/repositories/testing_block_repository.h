#pragma once

#include "core/result.h"
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

    [[nodiscard]] Result<QList<TestingAssignment>>
    loadTestingAssignments();

    [[nodiscard]] Result<QList<TestingAssignmentDisplayRecord>>
    loadTestingAssignmentDisplayRecords();
    [[nodiscard]] const TestingAssignmentDisplayReadMetrics&
    testingAssignmentDisplayReadMetrics() const noexcept;

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
    TestingAssignmentDisplayReadMetrics
        m_testingAssignmentDisplayReadMetrics;
};
