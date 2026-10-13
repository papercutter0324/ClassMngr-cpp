#include "native_english_teacher_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

#include <limits>

namespace
{
QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

NativeEnglishTeacherDirectorySaveOutcome queryError(
    const QSqlQuery& query,
    const QString& action
    )
{
    return {
        .code = NativeEnglishTeacherDirectorySaveOutcomeCode::Failure,
        .diagnostic = SqlQueryUtils::errorFor(query, action).userMessage()
    };
}
}

NativeEnglishTeacherRepository::NativeEnglishTeacherRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<QList<NativeEnglishTeacher>> NativeEnglishTeacherRepository::getAll() const
{
    QList<NativeEnglishTeacher> result;
    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
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
    )"),
        QObject::tr("Loading Native English Teacher directory")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool idIsInteger = false;
        const qlonglong persistedId = query.value(0).toLongLong(&idIsInteger);
        if (!idIsInteger || persistedId <= 0
            || persistedId > std::numeric_limits<int>::max())
        {
            return std::unexpected(
                idIsInteger
                    ? QObject::tr(
                        "Native English Teacher id %1 is outside the supported positive 32-bit range."
                        ).arg(persistedId)
                    : QObject::tr(
                        "A Native English Teacher id is not a valid 64-bit integer."
                        )
                );
        }

        NativeEnglishTeacher teacher;
        teacher.id = static_cast<int>(persistedId);
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

Result<QList<NativeEnglishTeacherBirthdayReadRecord>>
NativeEnglishTeacherRepository::loadBirthdayDirectoryReadRecords() const
{
    QList<NativeEnglishTeacherBirthdayReadRecord> result;
    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
        SELECT name, position, birthday
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
    )"),
        QObject::tr("Loading Native English Teacher directory")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        result.append({
            query.value(QStringLiteral("name")).toString(),
            query.value(QStringLiteral("position")).toString(),
            query.value(QStringLiteral("birthday")).toString()
        });
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(query.lastError().text());
    }

    return result;
}

Status NativeEnglishTeacherRepository::saveDirectory(
    const QList<NativeEnglishTeacher>& teachers,
    const QList<int>& deletedIds
    )
{
    const NativeEnglishTeacherDirectorySaveOutcome outcome =
        saveDirectoryWithOutcome(teachers, deletedIds);
    if (outcome.code != NativeEnglishTeacherDirectorySaveOutcomeCode::Saved)
    {
        return std::unexpected(outcome.diagnostic);
    }

    return {};
}

NativeEnglishTeacherDirectorySaveOutcome
NativeEnglishTeacherRepository::saveDirectoryWithOutcome(
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
            return {
                .code = NativeEnglishTeacherDirectorySaveOutcomeCode::Failure,
                .diagnostic = QObject::tr("Every Native English Teacher must have a name.")
            };
        }
        if (names.contains(name))
        {
            return {
                .code = NativeEnglishTeacherDirectorySaveOutcomeCode::Failure,
                .diagnostic = QObject::tr("Native English Teacher names must be unique.")
            };
        }
        names.insert(name);
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return {
            .code = NativeEnglishTeacherDirectorySaveOutcomeCode::Failure,
            .diagnostic = QObject::tr("Unable to start the directory save transaction.")
        };
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
        if (teacher.id > 0 && query.numRowsAffected() == 0)
        {
            return {
                .code = NativeEnglishTeacherDirectorySaveOutcomeCode::Missing,
                .diagnostic = QObject::tr(
                    "Saving Native English Teacher id %1 failed: no matching record exists."
                    ).arg(teacher.id)
            };
        }
    }

    if (!transaction.commit())
    {
        return {
            .code = NativeEnglishTeacherDirectorySaveOutcomeCode::Failure,
            .diagnostic = QObject::tr("Unable to commit the Native English Teacher directory.")
        };
    }

    return {.code = NativeEnglishTeacherDirectorySaveOutcomeCode::Saved};
}
