#pragma once

#include "core/result.h"
#include "domain/models/native_english_teacher.h"

#include <QList>
#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class NativeEnglishTeacherRepository
{
public:
    explicit NativeEnglishTeacherRepository(QSqlDatabase& database);
    ~NativeEnglishTeacherRepository();

    [[nodiscard]] Result<QList<NativeEnglishTeacher>> getAll() const;

    [[nodiscard]] Status saveDirectory(
        const QList<NativeEnglishTeacher>& teachers,
        const QList<int>& deletedIds
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        ) const;

    QSqlDatabase& m_database;
    mutable std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    mutable QString m_engineDatabasePath;
};
