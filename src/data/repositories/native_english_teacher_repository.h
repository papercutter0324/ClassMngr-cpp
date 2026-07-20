#pragma once

#include "core/result.h"
#include "domain/models/native_english_teacher.h"

#include <QList>
#include <QSqlDatabase>

class NativeEnglishTeacherRepository
{
public:
    explicit NativeEnglishTeacherRepository(QSqlDatabase& database);

    [[nodiscard]] QList<NativeEnglishTeacher> getAll() const;

    [[nodiscard]] Status saveDirectory(
        const QList<NativeEnglishTeacher>& teachers,
        const QList<int>& deletedIds
        );

private:
    QSqlDatabase& m_database;
};
