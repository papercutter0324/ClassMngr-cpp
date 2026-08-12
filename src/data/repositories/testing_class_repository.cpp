#include "testing_class_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "domain/rules/schedule_value_parser.h"

#include <QObject>
#include <QPair>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
QString queryFailure(
    const QSqlQuery& query,
    const QString& action
    )
{
    return SqlQueryUtils::errorFor(query, action).userMessage();
}

Status validateTestingClass(
    const TestingClass& testingClass,
    bool requireId
    )
{
    if (requireId && testingClass.classId <= 0)
    {
        return std::unexpected(
            QObject::tr("A valid testing class is required.")
            );
    }

    if (testingClass.name.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr("Testing class name is required.")
            );
    }

    if (testingClass.grade.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr("Testing class grade is required.")
            );
    }

    if (testingClass.level.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr("Testing class level is required.")
            );
    }

    if (testingClass.room.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr("Testing class room is required.")
            );
    }

    return {};
}

TestingClass readTestingClass(
    const QSqlQuery& query
    )
{
    TestingClass testingClass;
    testingClass.classId =
        query.value(QStringLiteral("class_id")).toInt();
    testingClass.name =
        query.value(QStringLiteral("name")).toString();
    testingClass.grade =
        query.value(QStringLiteral("class_grade")).toString();
    testingClass.level =
        query.value(QStringLiteral("class_level")).toString();
    testingClass.room =
        query.value(QStringLiteral("room")).toString();
    testingClass.teacherId =
        query.value(QStringLiteral("teacher_id")).isNull()
            ? -1
            : query.value(QStringLiteral("teacher_id")).toInt();
    testingClass.classColor =
        query.value(QStringLiteral("class_color")).toString();
    testingClass.fontColor =
        query.value(QStringLiteral("font_color")).toString();
    testingClass.notes =
        query.value(QStringLiteral("notes")).toString();

    if (testingClass.classColor.trimmed().isEmpty())
    {
        testingClass.classColor = QStringLiteral("#FFFFFF");
    }
    if (testingClass.fontColor.trimmed().isEmpty())
    {
        testingClass.fontColor = QStringLiteral("#000000");
    }

    return testingClass;
}

QString testingClassSelect(
    const QString& suffix = {}
    )
{
    return QStringLiteral(R"(
        SELECT
            c.id AS class_id,
            c.name,
            tc.room,
            ci.teacher_id,
            ci.class_grade,
            ci.class_level,
            ci.class_color,
            ci.font_color,
            ci.notes
        FROM testing_classes tc
        JOIN classes c
        ON c.id = tc.class_id
        LEFT JOIN class_info ci
        ON ci.class_id = tc.class_id
    )") + suffix;
}
}

TestingClassRepository::TestingClassRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<int> TestingClassRepository::createTestingClass(
    const TestingClass& testingClass,
    const QString& assignmentDay,
    const QString& assignmentStartTime
    )
{
    const Status valid =
        validateTestingClass(
            testingClass,
            false
            );

    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const bool hasAssignment =
        !assignmentDay.trimmed().isEmpty()
        || !assignmentStartTime.trimmed().isEmpty();
    const auto canonicalDay =
        ScheduleValueParser::parseWeekday(assignmentDay);
    const auto normalizedStartTime =
        ScheduleValueParser::parseTime(assignmentStartTime);
    if (
        hasAssignment
        && (
            !canonicalDay
            || !normalizedStartTime
            )
        )
    {
        return std::unexpected(
            QObject::tr(
                "A testing assignment requires a valid weekday and start time."
                )
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Could not start the testing class transaction.")
            );
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO classes (name) VALUES (?)"));
    query.addBindValue(testingClass.name.trimmed());
    const auto executed = SqlQueryUtils::execute(
        query,
        QObject::tr("Creating the testing class")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    const int classId =
        query.lastInsertId().toInt();
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Creating the testing class did not return a valid ID.")
            );
    }

    query.prepare(R"(
        INSERT INTO class_info (
            class_id,
            teacher_id,
            class_grade,
            class_level,
            class_color,
            font_color,
            notes,
            time_filler_activities
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, '')
    )");
    query.addBindValue(classId);
    query.addBindValue(
        testingClass.teacherId > 0
            ? QVariant(testingClass.teacherId)
            : QVariant()
        );
    query.addBindValue(testingClass.grade.trimmed());
    query.addBindValue(testingClass.level.trimmed());
    query.addBindValue(testingClass.classColor.trimmed());
    query.addBindValue(testingClass.fontColor.trimmed());
    query.addBindValue(testingClass.notes);
    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Saving testing class details"))
            );
    }

    query.prepare(R"(
        INSERT INTO testing_classes (
            class_id,
            room
        )
        VALUES (?, ?)
    )");
    query.addBindValue(classId);
    query.addBindValue(testingClass.room.trimmed());
    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Saving the testing class room"))
            );
    }

    if (hasAssignment)
    {
        query.prepare(R"(
            INSERT INTO schedule_testing_blocks (
                day,
                start_time,
                room,
                class_id
            )
            VALUES (?, ?, '', ?)
        )");
        query.addBindValue(canonicalDay->text);
        query.addBindValue(normalizedStartTime->text);
        query.addBindValue(classId);
        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Assigning the new testing class")
                    )
                );
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing the testing class transaction failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    return classId;
}

Status TestingClassRepository::updateTestingClass(
    const TestingClass& testingClass
    )
{
    const Status valid =
        validateTestingClass(
            testingClass,
            true
            );

    if (!valid)
    {
        return valid;
    }

    const Result<bool> existing =
        isTestingClass(testingClass.classId);
    if (!existing)
    {
        return std::unexpected(existing.error());
    }
    if (!*existing)
    {
        return std::unexpected(
            QObject::tr("The testing class no longer exists.")
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Could not start the testing class transaction.")
            );
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE classes SET name=? WHERE id=?"));
    query.addBindValue(testingClass.name.trimmed());
    query.addBindValue(testingClass.classId);
    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Updating the testing class name"))
            );
    }

    query.prepare(R"(
        INSERT INTO class_info (
            class_id,
            teacher_id,
            class_grade,
            class_level,
            class_color,
            font_color,
            notes,
            time_filler_activities
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, '')
        ON CONFLICT(class_id)
        DO UPDATE SET
            teacher_id=excluded.teacher_id,
            class_grade=excluded.class_grade,
            class_level=excluded.class_level,
            class_color=excluded.class_color,
            font_color=excluded.font_color,
            notes=excluded.notes
    )");
    query.addBindValue(testingClass.classId);
    query.addBindValue(
        testingClass.teacherId > 0
            ? QVariant(testingClass.teacherId)
            : QVariant()
        );
    query.addBindValue(testingClass.grade.trimmed());
    query.addBindValue(testingClass.level.trimmed());
    query.addBindValue(testingClass.classColor.trimmed());
    query.addBindValue(testingClass.fontColor.trimmed());
    query.addBindValue(testingClass.notes);
    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Updating testing class details"))
            );
    }

    query.prepare(
        QStringLiteral("UPDATE testing_classes SET room=? WHERE class_id=?")
        );
    query.addBindValue(testingClass.room.trimmed());
    query.addBindValue(testingClass.classId);
    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Updating the testing class room"))
            );
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing the testing class transaction failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    return {};
}

Result<TestingClass> TestingClassRepository::loadTestingClass(
    int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("A valid testing class is required.")
            );
    }

    QSqlQuery query(m_database);
    query.prepare(
        testingClassSelect(
            QStringLiteral(" WHERE tc.class_id=?")
            )
        );
    query.addBindValue(classId);
    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Loading the testing class"))
            );
    }

    if (!query.next())
    {
        return std::unexpected(
            QObject::tr("The testing class was not found.")
            );
    }

    return readTestingClass(query);
}

Result<QList<TestingClass>>
TestingClassRepository::loadTestingClasses()
{
    QList<TestingClass> testingClasses;
    QSqlQuery query(m_database);
    if (!query.exec(
            testingClassSelect(
                QStringLiteral(
                    " ORDER BY ci.class_grade, ci.class_level, c.name, c.id"
                    )
                )
            ))
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Loading testing classes"))
            );
    }

    while (query.next())
    {
        testingClasses.append(readTestingClass(query));
    }

    return testingClasses;
}

Status TestingClassRepository::deleteTestingClass(
    int classId
    )
{
    const Result<bool> existing =
        isTestingClass(classId);
    if (!existing)
    {
        return std::unexpected(existing.error());
    }
    if (!*existing)
    {
        return std::unexpected(
            QObject::tr("The testing class no longer exists.")
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Could not start the testing class transaction.")
            );
    }

    QSqlQuery query(m_database);
    const QList<QPair<QString, QString>> deletions{
        {
            QStringLiteral("DELETE FROM schedule_testing_blocks WHERE class_id=?"),
            QObject::tr("Removing testing assignments")
        },
        {
            QStringLiteral("DELETE FROM roster_columns WHERE class_id=?"),
            QObject::tr("Removing roster columns")
        },
        {
            QStringLiteral("DELETE FROM roster_data WHERE class_id=?"),
            QObject::tr("Removing roster data")
        },
        {
            QStringLiteral(
                "DELETE FROM speaking_eval_data "
                "WHERE evaluation_id IN ("
                "SELECT id FROM speaking_evaluations WHERE class_id=?"
                ")"
                ),
            QObject::tr("Removing speaking evaluation data")
        },
        {
            QStringLiteral("DELETE FROM speaking_evaluations WHERE class_id=?"),
            QObject::tr("Removing speaking evaluations")
        },
        {
            QStringLiteral("DELETE FROM class_times WHERE class_id=?"),
            QObject::tr("Removing regular class times")
        },
        {
            QStringLiteral("DELETE FROM class_intensive_times WHERE class_id=?"),
            QObject::tr("Removing intensive class times")
        },
        {
            QStringLiteral("DELETE FROM class_info WHERE class_id=?"),
            QObject::tr("Removing class details")
        },
        {
            QStringLiteral("DELETE FROM testing_classes WHERE class_id=?"),
            QObject::tr("Removing the testing class profile")
        },
        {
            QStringLiteral("DELETE FROM classes WHERE id=?"),
            QObject::tr("Removing the testing class")
        }
    };

    for (const auto& deletion : deletions)
    {
        query.prepare(deletion.first);
        query.addBindValue(classId);
        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(query, deletion.second)
                );
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing the testing class deletion failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    return {};
}

Result<bool> TestingClassRepository::isTestingClass(
    int classId
    )
{
    if (classId <= 0)
    {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral(
            "SELECT 1 FROM testing_classes WHERE class_id=?"
            )
        );
    query.addBindValue(classId);
    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Checking the testing class"))
            );
    }

    return query.next();
}
