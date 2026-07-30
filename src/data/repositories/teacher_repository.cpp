#include "teacher_repository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace
{
QString normalizedTeacherChoice(
    const QString& value,
    const QStringList& choices,
    const QString& fallback
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

    return fallback;
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
        },
        QStringLiteral("WiFi")
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
        },
        QStringLiteral("HDMI")
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
}

TeacherRepository::TeacherRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

int TeacherRepository::createTeacher(
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

    query.exec();

    return query.lastInsertId().toInt();
}

int TeacherRepository::saveTeacher(
    const Teacher& teacher
    )
{
    if (teacher.id > 0)
    {
        updateTeacher(teacher);

        return teacher.id;
    }

    return createTeacher(teacher);
}

void TeacherRepository::updateTeacher(
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

    query.exec();
}

Teacher TeacherRepository::getTeacher(
    int teacherId
    )
{
    Teacher teacher;

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT *
        FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load teacher:"
            << query.lastError().text();

        return teacher;
    }

    if (!query.next())
    {
        return teacher;
    }

    return teacherFromQuery(query);
}

QList<Teacher> TeacherRepository::getAllTeachers()
{
    QList<Teacher> teachers;

    QSqlQuery query(m_database);

    if (!query.exec(R"(
        SELECT *
        FROM teachers
        ORDER BY teacher_en
    )"))
    {
        qWarning()
            << "Failed to load teachers:"
            << query.lastError().text();

        return teachers;
    }

    while (query.next())
    {
        teachers.append(
            teacherFromQuery(query)
            );
    }

    return teachers;
}

void TeacherRepository::deleteTeacher(
    int teacherId
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        UPDATE class_info
        SET teacher_id=NULL
        WHERE teacher_id=?
    )");

    query.addBindValue(teacherId);

    query.exec();

    query.prepare(R"(
        DELETE FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    query.exec();
}
