#pragma once

#include "core/result.h"
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

    [[nodiscard]] Result<QList<NativeEnglishTeacher>> getAll() const;
    [[nodiscard]] Result<QList<NativeEnglishTeacherBirthdayReadRecord>>
        loadBirthdayDirectoryReadRecords() const;

    [[nodiscard]] Status saveDirectory(
        const QList<NativeEnglishTeacher>& teachers,
        const QList<int>& deletedIds
        );

private:
    QSqlDatabase& m_database;
};
