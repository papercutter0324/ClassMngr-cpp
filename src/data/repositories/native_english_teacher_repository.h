#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/native_english_teacher.h"

#include <QList>
#include <QSqlDatabase>

struct NativeEnglishTeacherBirthdayReadRecord final
{
    QString name;
    QString position;
    QString birthday;
};

class NativeEnglishTeacherRepository
{
public:
    explicit NativeEnglishTeacherRepository(QSqlDatabase& database);

    [[nodiscard]] RepositoryResult<QList<NativeEnglishTeacher>> getAll() const;
    [[nodiscard]] RepositoryResult<QList<NativeEnglishTeacherBirthdayReadRecord>>
        loadBirthdayDirectoryReadRecords() const;

    [[nodiscard]] RepositoryStatus saveDirectory(
        const QList<NativeEnglishTeacher>& teachers,
        const QList<int>& deletedIds
        );

private:
    QSqlDatabase& m_database;
};
