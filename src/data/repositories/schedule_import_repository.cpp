#include "schedule_import_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/classes/config/class_info_config.h"
#include "features/schedule/services/schedule_import_plan_validator.h"
#include "features/schedule/services/schedule_import_matcher.h"
#include "features/schedule/services/schedule_import_state_validator.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTime>

#include <algorithm>

namespace
{
QString teacherKey(
    const QString& value
    )
{
    return TeacherImportNameUtils::hangulOnly(value);
}

QString queryFailure(
    const QSqlQuery& query,
    const QString& action
    )
{
    return SqlQueryUtils::errorFor(query, action).userMessage();
}

int dayIndex(
    const QString& day
    )
{
    static const QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    return days.indexOf(day);
}

QString normalizedHexColor(
    const QString& value
    )
{
    static const QRegularExpression expression(
        QStringLiteral("^#[0-9A-Fa-f]{6}$")
        );
    const QString color =
        value.trimmed();
    return expression.match(color).hasMatch()
        ? color.toUpper()
        : QString();
}

QList<ClassTime> selectedTimes(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    return kind == ScheduleImportKind::Intensive
        ? info.intensiveTimes
        : info.classTimes;
}

Status writeTimes(
    QSqlDatabase& database,
    const QString& table,
    int classId,
    const QList<ClassTime>& times
    )
{
    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "INSERT INTO %1 "
            "(class_id, day, start_time, end_time) "
            "VALUES (?, ?, ?, ?)"
            )
            .arg(table)
        );

    for (const ClassTime& time : times)
    {
        query.bindValue(0, classId);
        query.bindValue(1, time.day);
        query.bindValue(2, time.startTime);
        query.bindValue(3, time.endTime);

        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Writing imported class times")
                    )
                );
        }
    }

    return {};
}

Status writeIntensiveSlotStates(
    QSqlDatabase& database,
    const QList<IntensiveSlotState>& states
    )
{
    static const QSet<QString> validStates{
        QStringLiteral("empty"),
        QStringLiteral("essay"),
        QStringLiteral("lunch")
    };

    QSet<QString> keys;
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO intensive_slot_states (day, start_time, state)
        VALUES (?, ?, ?)
    )");

    for (const IntensiveSlotState& state : states)
    {
        const int day = dayIndex(state.day);
        const QTime startTime =
            QTime::fromString(
                state.startTime,
                QStringLiteral("HH:mm")
                );
        const QString key =
            state.day + QLatin1Char('\x1f') + state.startTime;
        if (
            day < 0
            || !startTime.isValid()
            || !validStates.contains(state.state)
            || keys.contains(key)
            )
        {
            return std::unexpected(
                QObject::tr("The import contains an invalid intensive slot state.")
                );
        }
        keys.insert(key);

        query.bindValue(0, state.day);
        query.bindValue(1, state.startTime);
        query.bindValue(2, state.state);
        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Writing imported intensive slot states")
                    )
                );
        }
    }

    return {};
}
}

ScheduleImportRepository::ScheduleImportRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<ScheduleImportPreview> ScheduleImportRepository::preview(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(
            QObject::tr("No Teacher Profile is open.")
            );
    }

    TeacherRepository teacherRepository(m_database);
    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    const Result<QList<Teacher>> teachers =
        teacherRepository.getAllTeachers();
    if (!teachers)
    {
        return std::unexpected(teachers.error());
    }

    const Result<QList<Classroom>> classrooms =
        classRepository.getClasses();
    if (!classrooms)
    {
        return std::unexpected(classrooms.error());
    }

    QHash<int, ClassInfo> classInfo;
    for (const Classroom& classroom : *classrooms)
    {
        classInfo.insert(
            classroom.id,
            classInfoRepository.loadClassInfo(classroom.id)
            );
    }

    return ScheduleImportMatcher::preview(
        user,
        kind,
        *teachers,
        *classrooms,
        classInfo
        );
}

Result<ScheduleImportSummary> ScheduleImportRepository::apply(
    const ScheduleImportPlan& plan
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(
            QObject::tr("No Teacher Profile is open.")
            );
    }

    const Result<ValidatedScheduleImportPlan> validatedPlan =
        ScheduleImportPlanValidator::validate(plan);
    if (!validatedPlan)
    {
        return std::unexpected(validatedPlan.error());
    }
    const auto& teacherResolutions =
        validatedPlan->teacherResolutions;
    const auto& classResolutions =
        validatedPlan->classResolutions;

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr(
                "Unable to start the schedule import transaction."
                )
            );
    }

    TeacherRepository teacherRepository(m_database);
    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    const Result<QList<Teacher>> existingTeachers =
        teacherRepository.getAllTeachers();
    if (!existingTeachers)
    {
        return std::unexpected(existingTeachers.error());
    }

    const Result<QList<Classroom>> existingClasses =
        classRepository.getClasses();
    if (!existingClasses)
    {
        return std::unexpected(existingClasses.error());
    }

    QHash<int, ClassInfo> existingInfo;
    for (const Classroom& classroom : *existingClasses)
    {
        existingInfo.insert(
            classroom.id,
            classInfoRepository.loadClassInfo(classroom.id)
            );
    }

    const Status currentState = ScheduleImportStateValidator::validate(
        plan,
        *validatedPlan,
        *existingTeachers,
        *existingClasses,
        existingInfo
        );
    if (!currentState)
    {
        return std::unexpected(currentState.error());
    }

    ScheduleImportSummary summary;
    summary.ignoredCells =
        plan.diagnostics.size();
    QHash<QString, int> resolvedTeacherIds;
    QSqlQuery query(m_database);

    for (
        auto iterator = teacherResolutions.cbegin();
        iterator != teacherResolutions.cend();
        ++iterator
        )
    {
        const ScheduleImportTeacherResolution& resolution =
            iterator.value();

        if (
            resolution.action
                == ScheduleImportTeacherAction::Skip
            )
        {
            resolvedTeacherIds.insert(iterator.key(), -1);
            continue;
        }

        if (
            resolution.action
                == ScheduleImportTeacherAction::Create
            )
        {
            QString teacherName;
            for (const ScheduleImportClassCandidate& candidate : plan.candidates)
            {
                if (candidate.teacherKey == iterator.key())
                {
                    teacherName = candidate.teacherKr;
                    break;
                }
            }
            teacherName =
                teacherKey(teacherName);

            query.prepare(R"(
                INSERT INTO teachers (
                    teacher_kr,
                    room_number
                )
                VALUES (?, ?)
            )");
            query.addBindValue(teacherName);
            query.addBindValue(
                resolution.selectedRoom.trimmed()
                );

            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Creating a Korean teacher")
                        )
                    );
            }

            const int teacherId =
                query.lastInsertId().toInt();
            if (teacherId <= 0)
            {
                return std::unexpected(
                    QObject::tr(
                        "A Korean teacher could not be created."
                        )
                    );
            }
            resolvedTeacherIds.insert(iterator.key(), teacherId);
            ++summary.teachersCreated;
            continue;
        }

        resolvedTeacherIds.insert(
            iterator.key(),
            resolution.targetTeacherId
            );

        if (
            resolution.action
                == ScheduleImportTeacherAction::UpdateRoom
            )
        {
            query.prepare(R"(
                UPDATE teachers
                SET room_number=?
                WHERE id=?
            )");
            query.addBindValue(
                resolution.selectedRoom.trimmed()
                );
            query.addBindValue(resolution.targetTeacherId);

            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Updating a Korean teacher room")
                        )
                    );
            }
            ++summary.teachersUpdated;
        }
    }

    const QString timeTable =
        plan.kind == ScheduleImportKind::Intensive
            ? QStringLiteral("class_intensive_times")
            : QStringLiteral("class_times");
    const bool preservesAbsentIntensiveClasses =
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode
            == ScheduleImportIntensiveMode::UpdateExisting;
    QHash<int, QList<ClassTime>> finalTimes;
    for (int index = 0; index < plan.candidates.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate =
            plan.candidates[index];
        const ScheduleImportClassResolution resolution =
            classResolutions.value(index);

        if (
            resolution.action
                == ScheduleImportClassAction::Skip
            )
        {
            ++summary.classesSkipped;
            if (
                !preservesAbsentIntensiveClasses
                && resolution.targetClassId > 0
                && existingInfo.contains(resolution.targetClassId)
                )
            {
                finalTimes.insert(
                    resolution.targetClassId,
                    selectedTimes(
                        existingInfo.value(
                            resolution.targetClassId
                            ),
                        plan.kind
                        )
                    );
            }
            continue;
        }

        const int teacherId =
            resolvedTeacherIds.value(
                candidate.teacherKey,
                -1
                );
        if (teacherId <= 0)
        {
            return std::unexpected(
                QObject::tr(
                    "A class cannot be imported because its Korean teacher was skipped."
                    )
                );
        }

        int classId =
            resolution.targetClassId;

        if (
            resolution.action
                == ScheduleImportClassAction::CreateNew
            )
        {
            query.prepare(
                QStringLiteral(
                    "INSERT INTO classes (name) VALUES (?)"
                    )
                );
            query.addBindValue(
                QStringLiteral("%1 %2")
                    .arg(
                        candidate.classGrade,
                        candidate.classLevel
                        )
                    .simplified()
                );
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Creating a class")
                        )
                    );
            }
            classId =
                query.lastInsertId().toInt();
            ++summary.classesCreated;
        }
        else
        {
            ++summary.classesUpdated;
        }

        query.prepare(R"(
            INSERT INTO class_info (
                class_id,
                teacher_id,
                class_grade,
                class_level,
                class_color,
                font_color
            )
            VALUES (?, ?, ?, ?, ?, ?)
            ON CONFLICT(class_id)
            DO UPDATE SET
                teacher_id=excluded.teacher_id,
                class_grade=excluded.class_grade,
                class_level=excluded.class_level,
                class_color=excluded.class_color,
                font_color=excluded.font_color
        )");
        query.addBindValue(classId);
        query.addBindValue(teacherId);
        query.addBindValue(candidate.classGrade);
        query.addBindValue(candidate.classLevel);
        query.addBindValue(
            normalizedHexColor(
                resolution.classColor
                )
            );
        query.addBindValue(
            normalizedHexColor(
                resolution.fontColor
                )
            );

        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Updating imported class information")
                    )
                );
        }

        finalTimes.insert(classId, candidate.times);
    }

    if (!preservesAbsentIntensiveClasses)
    {
        for (const Classroom& classroom : *existingClasses)
        {
            const bool hadTimes =
                !selectedTimes(
                    existingInfo.value(classroom.id),
                    plan.kind
                    ).isEmpty();
            if (
                hadTimes
                && !finalTimes.contains(classroom.id)
                )
            {
                ++summary.schedulesCleared;
            }
        }
    }

    if (preservesAbsentIntensiveClasses)
    {
        query.prepare(
            QStringLiteral(
                "DELETE FROM %1 WHERE class_id=?"
                )
                .arg(timeTable)
            );
        for (
            auto iterator = finalTimes.cbegin();
            iterator != finalTimes.cend();
            ++iterator
            )
        {
            query.bindValue(0, iterator.key());
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr(
                            "Clearing an existing intensive class schedule"
                            )
                        )
                    );
            }
        }
    }
    else if (
        !query.exec(
            QStringLiteral("DELETE FROM %1")
                .arg(timeTable)
            )
        )
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Clearing the previous schedule snapshot")
                )
            );
    }

    for (
        auto iterator = finalTimes.cbegin();
        iterator != finalTimes.cend();
        ++iterator
        )
    {
        const Status written =
            writeTimes(
                m_database,
                timeTable,
                iterator.key(),
                iterator.value()
                );
        if (!written)
        {
            return std::unexpected(written.error());
        }
    }

    if (plan.kind == ScheduleImportKind::Intensive)
    {
        if (!query.exec(QStringLiteral("DELETE FROM intensive_slot_states")))
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Clearing the previous intensive slot states")
                    )
                );
        }

        const Status statesWritten =
            writeIntensiveSlotStates(
                m_database,
                plan.intensiveSlotStates
                );
        if (!statesWritten)
        {
            return std::unexpected(statesWritten.error());
        }
    }

    if (
        plan.saveProfileNameIfBlank
        || plan.updateProfileName
        )
    {
        query.prepare(
            QStringLiteral(
                "SELECT value FROM app_settings WHERE key='myInfo/name'"
                )
            );
        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Reading My Information name")
                    )
                );
        }

        QString existingName;
        if (query.next())
        {
            existingName =
                query.value(0).toString().trimmed();
        }

        if (
            (
                existingName.isEmpty()
                || plan.updateProfileName
                )
            && !plan.selectedUserName.trimmed().isEmpty()
            )
        {
            query.prepare(R"(
                INSERT INTO app_settings (key, value)
                VALUES ('myInfo/name', ?)
                ON CONFLICT(key)
                DO UPDATE SET value=excluded.value
            )");
            query.addBindValue(
                plan.selectedUserName.trimmed()
                );
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Saving My Information name")
                        )
                    );
            }
            summary.profileNameUpdated = true;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr(
                "Unable to commit the schedule import transaction: %1"
                )
                .arg(m_database.lastError().text())
            );
    }

    return summary;
}
