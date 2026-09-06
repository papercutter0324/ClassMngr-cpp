#pragma once

#include "core/result.h"
#include "domain/models/classroom.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class ClassRepository
{
public:
    explicit ClassRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
    explicit ClassRepository(
        QSqlDatabase& database
        );
    ~ClassRepository();

    [[nodiscard]] Result<int> createClass(
        const QString& name
        );

    [[nodiscard]] Result<QList<Classroom>> getClasses();

    [[nodiscard]] Result<Classroom> getClassById(
        int classId
        );

    [[nodiscard]] Status updateClassName(
        int classId,
        const QString& name
        );

    [[nodiscard]] Status deleteClass(
        int classId
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation,
        const QString& classContext = {}
        );

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
