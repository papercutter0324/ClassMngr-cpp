#include "native_english_teacher_repository.h"

#include "data/database/database_transaction.h"

#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

Status queryError(const QSqlQuery& query, const QString& action)
{
    return std::unexpected(
        QObject::tr("%1 failed: %2").arg(action, query.lastError().text())
        );
}
}

NativeEnglishTeacherRepository::NativeEnglishTeacherRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

QList<NativeEnglishTeacher> NativeEnglishTeacherRepository::getAll() const
{
    QList<NativeEnglishTeacher> result;
    QSqlQuery query(m_database);

    if (!query.exec(R"(
        SELECT id, name, position, phone_number, birthday, nationality, email
        FROM native_english_teachers
        ORDER BY CASE position
            WHEN 'Co-ordinator' THEN 1
            WHEN 'Team Leader' THEN 2
            WHEN 'M3 Song''s' THEN 3
            WHEN 'M2 Song''s' THEN 4
            WHEN 'M1 Song''s' THEN 5
            WHEN 'E6 Song''s' THEN 6
            WHEN 'E5 Athena' THEN 7
            WHEN 'NET' THEN 8
            ELSE 9
        END, name COLLATE NOCASE, id
    )"))
    {
        return result;
    }

    while (query.next())
    {
        NativeEnglishTeacher teacher;
        teacher.id = query.value(0).toInt();
        teacher.name = query.value(1).toString();
        teacher.position = query.value(2).toString();
        teacher.phoneNumber = query.value(3).toString();
        teacher.birthday = query.value(4).toString();
        teacher.nationality = query.value(5).toString();
        teacher.email = query.value(6).toString();
        result.append(teacher);
    }

    return result;
}

Status NativeEnglishTeacherRepository::saveDirectory(
    const QList<NativeEnglishTeacher>& teachers,
    const QList<int>& deletedIds
    )
{
    QSet<QString> names;

    for (const NativeEnglishTeacher& teacher : teachers)
    {
        const QString name = normalizedName(teacher.name);
        if (name.isEmpty())
        {
            return std::unexpected(QObject::tr("Every Native English Teacher must have a name."));
        }
        if (names.contains(name))
        {
            return std::unexpected(QObject::tr("Native English Teacher names must be unique."));
        }
        names.insert(name);
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(QObject::tr("Unable to start the directory save transaction."));
    }

    for (int id : deletedIds)
    {
        if (id <= 0)
        {
            continue;
        }
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("DELETE FROM native_english_teachers WHERE id=?"));
        query.addBindValue(id);
        if (!query.exec())
        {
            return queryError(query, QObject::tr("Deleting a Native English Teacher"));
        }
    }

    for (const NativeEnglishTeacher& source : teachers)
    {
        NativeEnglishTeacher teacher = source;
        teacher.name = teacher.name.simplified();
        teacher.position = teacher.position.trimmed();
        teacher.phoneNumber = teacher.phoneNumber.trimmed();
        teacher.birthday = teacher.birthday.trimmed();
        teacher.nationality = teacher.nationality.trimmed();
        teacher.email = teacher.email.trimmed();

        QSqlQuery query(m_database);
        if (teacher.id > 0)
        {
            query.prepare(R"(
                UPDATE native_english_teachers
                SET name=?, position=?, phone_number=?, birthday=?, nationality=?, email=?
                WHERE id=?
            )");
            query.addBindValue(teacher.name);
            query.addBindValue(teacher.position);
            query.addBindValue(teacher.phoneNumber);
            query.addBindValue(teacher.birthday);
            query.addBindValue(teacher.nationality);
            query.addBindValue(teacher.email);
            query.addBindValue(teacher.id);
        }
        else
        {
            query.prepare(R"(
                INSERT INTO native_english_teachers
                    (name, position, phone_number, birthday, nationality, email)
                VALUES (?, ?, ?, ?, ?, ?)
            )");
            query.addBindValue(teacher.name);
            query.addBindValue(teacher.position);
            query.addBindValue(teacher.phoneNumber);
            query.addBindValue(teacher.birthday);
            query.addBindValue(teacher.nationality);
            query.addBindValue(teacher.email);
        }

        if (!query.exec())
        {
            return queryError(query, QObject::tr("Saving a Native English Teacher"));
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(QObject::tr("Unable to commit the Native English Teacher directory."));
    }

    return {};
}
