#pragma once

#include "core/result.h"
#include "domain/models/classroom.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

class ClassRepository
{
public:
    enum class ClassByIdLookupCode
    {
        Found,
        Missing,
        Failure
    };

    struct ClassByIdLookupResult final
    {
        ClassByIdLookupCode code = ClassByIdLookupCode::Failure;
        Classroom classroom;
        QString diagnostic;
    };

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

    [[nodiscard]] Result<int> createClass(
        const QString& name
        );

    [[nodiscard]] Result<QList<Classroom>> getClasses();

    [[nodiscard]] Result<Classroom> getClassById(
        int classId
        );
    [[nodiscard]] ClassByIdLookupResult lookupClassById(
        int classId
        );
    [[nodiscard]] Result<QList<Classroom>> getClassesByIds(
        const QList<int>& classIds
        );
    [[nodiscard]] const ReadMetrics& readMetrics() const noexcept;

    [[nodiscard]] Status updateClassName(
        int classId,
        const QString& name
        );

    [[nodiscard]] Status deleteClass(
        int classId
        );

private:
    QSqlDatabase& m_database;
    ReadMetrics m_readMetrics;
};
