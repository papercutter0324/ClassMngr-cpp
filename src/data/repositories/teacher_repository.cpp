#include "teacher_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace
{
QString normalizedTeacherChoice(
    const QString& value,
    const QStringList& choices
    )
{
    const QString trimmed =
        value.trimmed();

    for (const QString& choice : choices)
    {
        if (choice.compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return choice;
        }
    }

    // Keep legacy or invalid values observable. Feature services validate new
    // writes, and silently turning an unknown value into a default hides data
    // corruption from callers.
    return trimmed;
}

QString normalizedInternetType(
    const QString& value
    )
{
    return normalizedTeacherChoice(
        value,
        {
            QStringLiteral("WiFi"),
            QStringLiteral("LAN"),
            QStringLiteral("Both"),
            QStringLiteral("N/A")
        }
        );
}

QString normalizedProjectionType(
    const QString& value
    )
{
    return normalizedTeacherChoice(
        value,
        {
            QStringLiteral("HDMI"),
            QStringLiteral("Zoom"),
            QStringLiteral("Any"),
            QStringLiteral("N/A")
        }
        );
}

Teacher teacherFromQuery(
    const QSqlQuery& query
    )
{
    Teacher teacher;

    teacher.id =           query.value("id").toInt();
    teacher.teacherKr =    query.value("teacher_kr").toString();
    teacher.teacherEn =    query.value("teacher_en").toString();
    teacher.preferredRomanization =
        query.value("preferred_romanization").toString();
    teacher.preferredName =
        query.value("preferred_name").toString();
    teacher.roomNumber =   query.value("room_number").toString();
    teacher.birthday =     query.value("birthday").toString();
    teacher.phoneNumber =  query.value("phone_number").toString();
    teacher.wifiName =     query.value("wifi_name").toString();
    teacher.wifiPassword = query.value("wifi_password").toString();
    teacher.internetType =
        normalizedInternetType(
            query.value("internet_type").toString()
            );
    teacher.zoomId =       query.value("zoom_id").toString();
    teacher.zoomPassword = query.value("zoom_password").toString();
    teacher.projectionType =
        normalizedProjectionType(
            query.value("projection_type").toString()
            );
    teacher.notes =        query.value("notes").toString();

    return teacher;
}

QString teacherIdentity(int teacherId)
{
    return QObject::tr("teacher id %1").arg(teacherId);
}

Status statusFromExecution(
    const SqlQueryUtils::ExecutionResult& result
    )
{
    if (!result)
    {
        return std::unexpected(result.error().userMessage());
    }

    return {};
}
}

TeacherRepository::TeacherRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<int> TeacherRepository::createTeacher(
    const Teacher& teacher
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        INSERT INTO teachers (
            teacher_kr,
            teacher_en,
            preferred_romanization,
            preferred_name,
            room_number,
            birthday,
            phone_number,
            wifi_name,
            wifi_password,
            internet_type,
            zoom_id,
            zoom_password,
            projection_type,
            notes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.preferredRomanization);
    query.addBindValue(teacher.preferredName);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.birthday);
    query.addBindValue(teacher.phoneNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(
        normalizedInternetType(teacher.internetType));
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(
        normalizedProjectionType(teacher.projectionType));
    query.addBindValue(teacher.notes);

    const QString identity = QObject::tr("teacher '%1'")
        .arg(
            teacher.teacherEn.trimmed().isEmpty()
                ? teacher.teacherKr.trimmed()
                : teacher.teacherEn.trimmed()
            );
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Creating teacher"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    const int teacherId = query.lastInsertId().toInt();
    if (teacherId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Creating %1 failed: the database did not return a valid "
                "record id."
                ).arg(identity)
            );
    }

    return teacherId;
}

Result<int> TeacherRepository::saveTeacher(
    const Teacher& teacher
    )
{
    if (teacher.id > 0)
    {
        const Status updated = updateTeacher(teacher);
        if (!updated)
        {
            return std::unexpected(updated.error());
        }

        return teacher.id;
    }

    return createTeacher(teacher);
}

Status TeacherRepository::updateTeacher(
    const Teacher& teacher
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        UPDATE teachers
        SET
            teacher_kr=?,
            teacher_en=?,
            preferred_romanization=?,
            preferred_name=?,
            room_number=?,
            birthday=?,
            phone_number=?,
            wifi_name=?,
            wifi_password=?,
            internet_type=?,
            zoom_id=?,
            zoom_password=?,
            projection_type=?,
            notes=?
        WHERE id=?
    )");

    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.preferredRomanization);
    query.addBindValue(teacher.preferredName);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.birthday);
    query.addBindValue(teacher.phoneNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(
        normalizedInternetType(teacher.internetType));
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(
        normalizedProjectionType(teacher.projectionType));
    query.addBindValue(teacher.notes);
    query.addBindValue(teacher.id);

    return statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Updating teacher"),
            teacherIdentity(teacher.id)
            )
        );
}

Result<Teacher> TeacherRepository::getTeacher(
    int teacherId
    )
{
    if (teacherId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading teacher failed: invalid teacher id %1.")
                .arg(teacherId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT *
        FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading teacher"),
        teacherIdentity(teacherId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        return std::unexpected(
            QObject::tr(
                "Loading teacher failed for %1: no matching record exists."
                ).arg(teacherIdentity(teacherId))
            );
    }

    return teacherFromQuery(query);
}

Result<QList<Teacher>> TeacherRepository::getAllTeachers()
{
    QList<Teacher> teachers;

    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
        SELECT *
        FROM teachers
        ORDER BY teacher_en
    )"),
        QObject::tr("Loading teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        teachers.append(
            teacherFromQuery(query)
            );
    }

    return teachers;
}

Status TeacherRepository::deleteTeacher(
    int teacherId
    )
{
    if (teacherId <= 0)
    {
        return std::unexpected(
            QObject::tr("Deleting teacher failed: invalid teacher id %1.")
                .arg(teacherId)
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting teacher deletion transaction failed for %1: %2")
                .arg(teacherIdentity(teacherId), m_database.lastError().text())
            );
    }

    const QString identity = teacherIdentity(teacherId);
    QSqlQuery query(m_database);

    query.prepare(R"(
        UPDATE class_info
        SET teacher_id=NULL
        WHERE teacher_id=?
    )");

    query.addBindValue(teacherId);

    Status status = statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Removing teacher from classes"),
            identity
            )
        );
    if (!status)
    {
        return status;
    }

    query.prepare(R"(
        DELETE FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    status = statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Deleting teacher"),
            identity
            )
        );
    if (!status)
    {
        return status;
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing teacher deletion failed for %1: %2")
                .arg(identity, m_database.lastError().text())
            );
    }

    return {};
}
