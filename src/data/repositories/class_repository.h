#pragma once

#include "core/result.h"
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
