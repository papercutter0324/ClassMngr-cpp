#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/classroom.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

class ClassRepository
{
public:
    struct ReadMetrics final
    {
        int getClassByIdCallCount = 0;
        int getClassesByIdsCallCount = 0;
        int requestedClassCount = 0;
        int batchStatementCount = 0;
        int getClassesCallCount = 0;
        int getClassesStatementAttemptCount = 0;
        // Calls and statement attempts include failures; rows count only for
        // reads that complete and validate successfully.
        int getClassesReturnedRowCount = 0;
    };

    explicit ClassRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryResult<int> createClass(
        const QString& name
        );

    [[nodiscard]] RepositoryResult<QList<Classroom>> getClasses();
    // Bounded ordered ID/name projection for compact navigation surfaces.
    // Uses a one-row lookahead and rejects over-limit or unbounded names.
    [[nodiscard]] RepositoryResult<QList<Classroom>>
        getBoundedClassesForNavigation(
            int maximumClassCount,
            int maximumNameLength
            );

    [[nodiscard]] RepositoryResult<Classroom> getClassById(
        int classId
        );
    [[nodiscard]] RepositoryResult<Classroom> lookupClassById(
        int classId
        );
    [[nodiscard]] RepositoryResult<QList<Classroom>> getClassesByIds(
        const QList<int>& classIds
        );
    [[nodiscard]] const ReadMetrics& readMetrics() const noexcept;

    [[nodiscard]] RepositoryStatus updateClassName(
        int classId,
        const QString& name
        );

    [[nodiscard]] RepositoryStatus deleteClass(
        int classId
        );

private:
    QSqlDatabase& m_database;
    ReadMetrics m_readMetrics;
};
