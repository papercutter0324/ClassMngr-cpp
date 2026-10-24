#include "testing_class_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "domain/rules/schedule_value_parser.h"

#include <QObject>
#include <QPair>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <limits>

namespace
{
RepositoryError queryFailure(
    const QSqlQuery& query,
    const QString& action,
    const RepositoryErrorCode code = RepositoryErrorCode::Technical
    )
{
    return RepositoryError{
        code,
        SqlQueryUtils::errorFor(query, action).userMessage()
    };
}

bool isUniqueConstraintViolation(const QSqlError& error)
{
    const QString nativeCodeText = error.nativeErrorCode();
    bool codeOk = false;
    const int nativeCode = nativeCodeText.toInt(&codeOk, 10);
    return codeOk
        && nativeCode == 2067
        && nativeCodeText == QStringLiteral("2067");
}

RepositoryStatus validateTestingClass(
    const TestingClass& testingClass,
    bool requireId
    )
{
    if (requireId && testingClass.classId <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr("A valid testing class is required.")
        });
    }

    if (testingClass.name.trimmed().isEmpty())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("Testing class name is required.")
        });
    }

    if (testingClass.grade.trimmed().isEmpty())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("Testing class grade is required.")
        });
    }

    if (testingClass.level.trimmed().isEmpty())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("Testing class level is required.")
        });
    }

    if (testingClass.room.trimmed().isEmpty())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("Testing class room is required.")
        });
    }

    return {};
}

RepositoryResult<TestingClass> readTestingClass(
    const QSqlQuery& query
    )
{
    const QVariant storedClassId = query.value(QStringLiteral("class_id"));
    bool classIdOk = false;
    const qint64 storedClassIdValue =
        storedClassId.toLongLong(&classIdOk);
    if (
        !classIdOk
        || storedClassIdValue <= 0
        || storedClassIdValue > std::numeric_limits<int>::max()
        )
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("The testing class has an invalid class identifier.")
        });
    }

    TestingClass testingClass;
    testingClass.classId = static_cast<int>(storedClassIdValue);
    testingClass.name =
        query.value(QStringLiteral("name")).toString();
    testingClass.grade =
        query.value(QStringLiteral("class_grade")).toString();
    testingClass.level =
        query.value(QStringLiteral("class_level")).toString();
    testingClass.room =
        query.value(QStringLiteral("room")).toString();
    const QVariant storedTeacherId = query.value(QStringLiteral("teacher_id"));
    if (!storedTeacherId.isNull())
    {
        bool teacherIdOk = false;
        testingClass.teacherId = storedTeacherId.toLongLong(&teacherIdOk);
        if (!teacherIdOk)
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr("The testing class has an invalid teacher identifier.")
            });
        }
    }
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

RepositoryResult<TestingClassDetailsReadRecord> readTestingClassDetails(
    const QSqlQuery& query
    )
{
    const QVariant storedTeacherId =
        query.value(QStringLiteral("teacher_id"));
    qint64 teacherId = -1;
    if (!storedTeacherId.isNull())
    {
        bool teacherIdOk = false;
        teacherId = storedTeacherId.toLongLong(&teacherIdOk);
        if (!teacherIdOk)
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "The testing class has an invalid teacher identifier."
                    )
            });
        }
    }

    bool classIdOk = false;
    const qint64 classId =
        query.value(QStringLiteral("class_id")).toLongLong(&classIdOk);
    if (!classIdOk || classId <= 0 || classId > std::numeric_limits<int>::max())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("The testing class has an invalid class identifier.")
        });
    }

    TestingClassDetailsReadRecord record{
        .classId = static_cast<int>(classId),
        .name = query.value(QStringLiteral("name")).toString(),
        .grade = query.value(QStringLiteral("class_grade")).toString(),
        .level = query.value(QStringLiteral("class_level")).toString(),
        .room = query.value(QStringLiteral("room")).toString(),
        .teacherId = teacherId,
        .classColor = query.value(QStringLiteral("class_color")).toString(),
        .fontColor = query.value(QStringLiteral("font_color")).toString(),
        .notes = query.value(QStringLiteral("notes")).toString()
    };

    if (record.classColor.trimmed().isEmpty())
    {
        record.classColor = QStringLiteral("#FFFFFF");
    }
    if (record.fontColor.trimmed().isEmpty())
    {
        record.fontColor = QStringLiteral("#000000");
    }

    return record;
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

QString testingClassDetailsReadSelect()
{
    return QStringLiteral(R"(
        SELECT
            c.id AS class_id,
            c.name,
            tc.class_id AS testing_class_id,
            tc.room,
            ci.teacher_id,
            ci.class_grade,
            ci.class_level,
            ci.class_color,
            ci.font_color,
            ci.notes
        FROM classes c
        LEFT JOIN testing_classes tc
        ON tc.class_id = c.id
        LEFT JOIN class_info ci
        ON ci.class_id = c.id
        WHERE c.id=?
    )");
}
}

TestingClassRepository::TestingClassRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

RepositoryResult<int> TestingClassRepository::createTestingClass(
    const TestingClass& testingClass,
    const QString& assignmentDay,
    const QString& assignmentStartTime
    )
{
    const RepositoryStatus valid =
        validateTestingClass(
            testingClass,
            false
            );

    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const bool hasDay = !assignmentDay.trimmed().isEmpty();
    const bool hasStartTime = !assignmentStartTime.trimmed().isEmpty();
    const bool hasAssignment = hasDay && hasStartTime;
    if (hasDay != hasStartTime)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr(
                "A testing assignment requires a valid weekday and start time."
                )
        });
    }
    const auto canonicalDay =
        ScheduleValueParser::parseWeekday(assignmentDay);
    const auto normalizedStartTime =
        ScheduleValueParser::parseTime(assignmentStartTime);
    if (
        hasAssignment && (!canonicalDay || !normalizedStartTime)
        )
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr(
                "A testing assignment requires a valid weekday and start time."
                )
        });
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Could not start the testing class transaction.")
        });
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
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            executed.error().userMessage()
        });
    }

    bool classIdOk = false;
    const qint64 generatedClassId =
        query.lastInsertId().toLongLong(&classIdOk);
    if (!classIdOk || generatedClassId <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Creating the testing class did not return a valid ID.")
        });
    }
    if (generatedClassId > std::numeric_limits<int>::max())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr(
                "Creating the testing class returned an ID outside the "
                "supported integer range."
                ),
            false
        });
    }
    const int classId = static_cast<int>(generatedClassId);

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
                    QObject::tr("Assigning the new testing class"),
                    isUniqueConstraintViolation(query.lastError())
                        ? RepositoryErrorCode::Conflict
                        : RepositoryErrorCode::Technical
                    )
                );
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Committing the testing class transaction failed: %1")
                .arg(m_database.lastError().text())
        });
    }

    return classId;
}

RepositoryStatus TestingClassRepository::updateTestingClass(
    const TestingClass& testingClass
    )
{
    const RepositoryStatus valid =
        validateTestingClass(
            testingClass,
            true
            );

    if (!valid)
    {
        return valid;
    }

    const RepositoryResult<bool> existing =
        isTestingClass(testingClass.classId);
    if (!existing)
    {
        return std::unexpected(existing.error());
    }
    if (!*existing)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr("The testing class no longer exists.")
        });
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Could not start the testing class transaction.")
        });
    }

    const RepositoryStatus updated = writeTestingClass(testingClass);
    if (!updated)
    {
        return updated;
    }

    if (!transaction.commit())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Committing the testing class transaction failed: %1")
                .arg(m_database.lastError().text())
        });
    }

    return {};
}

RepositoryStatus
TestingClassRepository::updateTestingClassDetailed(
    const TestingClass& testingClass
    )
{
    const RepositoryStatus valid = validateTestingClass(testingClass, true);
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const QString identity =
        QObject::tr("class id %1").arg(testingClass.classId);
    DatabaseTransaction transaction(
        m_database,
        DatabaseTransaction::Mode::Immediate
        );
    if (!transaction.started())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Starting the testing class transaction failed for %1: %2"
                ).arg(identity, transaction.startError())
        });
    }

    QSqlQuery parentQuery(m_database);
    parentQuery.prepare(QStringLiteral(
        "SELECT 1 FROM classes WHERE id=?"
        ));
    parentQuery.addBindValue(testingClass.classId);
    const auto parentQueryResult = SqlQueryUtils::executePrepared(
        parentQuery,
        QObject::tr("Checking testing class parent class"),
        identity
        );
    if (!parentQueryResult)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            parentQueryResult.error().userMessage()
        });
    }

    const bool parentClassExists = parentQuery.next();
    const QSqlError parentFetchError = parentQuery.lastError();
    parentQuery.finish();
    if (parentFetchError.isValid())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Checking testing class parent class failed for %1: %2"
                ).arg(identity, parentFetchError.text())
        });
    }

    if (!parentClassExists)
    {
        if (!transaction.rollbackChecked())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Technical,
                QObject::tr(
                    "Rolling back the testing class transaction failed for "
                    "%1: %2"
                    ).arg(identity, m_database.lastError().text())
            });
        }

        return std::unexpected(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr(
                "The testing class parent class was not found for %1."
                ).arg(identity)
        });
    }

    QSqlQuery testingClassQuery(m_database);
    testingClassQuery.prepare(QStringLiteral(
        "SELECT 1 FROM testing_classes WHERE class_id=?"
        ));
    testingClassQuery.addBindValue(testingClass.classId);
    const auto testingClassQueryResult = SqlQueryUtils::executePrepared(
        testingClassQuery,
        QObject::tr("Checking testing class details target"),
        identity
        );
    if (!testingClassQueryResult)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            testingClassQueryResult.error().userMessage()
        });
    }

    const bool testingClassExists = testingClassQuery.next();
    const QSqlError testingClassFetchError = testingClassQuery.lastError();
    testingClassQuery.finish();
    if (testingClassFetchError.isValid())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Checking testing class details target failed for %1: %2"
                ).arg(identity, testingClassFetchError.text())
        });
    }

    if (!testingClassExists)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("The testing class no longer exists.")
        });
    }

    const RepositoryStatus updated = writeTestingClass(testingClass);
    if (!updated)
    {
        return std::unexpected(updated.error());
    }

    if (!transaction.commit())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Committing the testing class transaction failed: %1"
                ).arg(m_database.lastError().text())
        });
    }

    return {};
}

RepositoryStatus TestingClassRepository::writeTestingClass(
    const TestingClass& testingClass
    )
{
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

    return {};
}

RepositoryResult<TestingClass> TestingClassRepository::loadTestingClass(
    int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr("A valid testing class is required.")
        });
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
        if (query.lastError().isValid())
        {
            return std::unexpected(queryFailure(
                query,
                QObject::tr("Loading the testing class")
                ));
        }

        return std::unexpected(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr("The testing class was not found.")
        });
    }

    return readTestingClass(query);
}

RepositoryResult<TestingClassDetailsReadRecord>
TestingClassRepository::loadTestingClassDetailsReadRecord(
    int classId
    )
{
    const RepositoryResult<TestingClassDetailsReadRecord> detailedResult =
        loadTestingClassDetailsReadRecordDetailed(classId);
    if (!detailedResult)
    {
        return std::unexpected(detailedResult.error());
    }

    return *detailedResult;
}

RepositoryResult<TestingClassDetailsReadRecord>
TestingClassRepository::loadTestingClassDetailsReadRecordDetailed(
    int classId
    )
{
    ++m_testingClassDetailsReadMetrics.callCount;

    if (classId <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr("A valid testing class is required.")
        });
    }

    QSqlQuery query(m_database);
    query.prepare(testingClassDetailsReadSelect());
    query.addBindValue(classId);
    ++m_testingClassDetailsReadMetrics.statementCount;
    if (!query.exec())
    {
        return std::unexpected(queryFailure(
                query,
                QObject::tr("Loading the testing class")
                ));
    }

    if (!query.next())
    {
        const QSqlError fetchError = query.lastError();
        query.finish();
        if (fetchError.isValid())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Technical,
                QObject::tr(
                    "Loading the testing class failed: %1"
                    ).arg(fetchError.text())
            });
        }

        return std::unexpected(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr("The testing class was not found.")
        });
    }

    if (query.value(QStringLiteral("testing_class_id")).isNull())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("The testing class was not found.")
        });
    }

    const RepositoryResult<TestingClassDetailsReadRecord> details =
        readTestingClassDetails(query);
    if (!details)
    {
        return std::unexpected(details.error());
    }

    return *details;
}

const TestingClassDetailsReadMetrics&
TestingClassRepository::testingClassDetailsReadMetrics() const noexcept
{
    return m_testingClassDetailsReadMetrics;
}

RepositoryResult<QList<TestingClass>>
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
        const auto testingClass = readTestingClass(query);
        if (!testingClass)
        {
            return std::unexpected(testingClass.error());
        }
        testingClasses.append(*testingClass);
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Loading testing classes"))
            );
    }

    return testingClasses;
}

RepositoryResult<QList<TestingClassChoiceRecord>>
TestingClassRepository::loadTestingClassChoices()
{
    ++m_testingClassChoicesReadMetrics.callCount;

    QList<TestingClassChoiceRecord> choices;
    QSqlQuery query(m_database);
    ++m_testingClassChoicesReadMetrics.statementCount;
    if (!query.exec(QStringLiteral(R"(
            SELECT
                c.id AS class_id,
                c.name,
                ci.class_grade,
                ci.class_level,
                tc.room
            FROM testing_classes tc
            JOIN classes c ON c.id = tc.class_id
            LEFT JOIN class_info ci ON ci.class_id = tc.class_id
            ORDER BY ci.class_grade, ci.class_level, c.name, c.id
        )")))
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Loading testing classes"))
            );
    }

    while (query.next())
    {
        const QVariant storedClassId =
            query.value(QStringLiteral("class_id"));
        bool classIdOk = false;
        const qint64 storedClassIdValue =
            storedClassId.toLongLong(&classIdOk);
        if (
            !classIdOk
            || storedClassIdValue <= 0
            || storedClassIdValue > std::numeric_limits<int>::max()
            )
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading testing classes found an invalid class identifier."
                    )
            });
        }

        choices.append({
            .classId = static_cast<int>(storedClassIdValue),
            .name = query.value(QStringLiteral("name")).toString(),
            .grade = query.value(QStringLiteral("class_grade")).toString(),
            .level = query.value(QStringLiteral("class_level")).toString(),
            .room = query.value(QStringLiteral("room")).toString()
        });
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(
            queryFailure(query, QObject::tr("Loading testing classes"))
            );
    }

    return choices;
}

const TestingClassChoicesReadMetrics&
TestingClassRepository::testingClassChoicesReadMetrics() const noexcept
{
    return m_testingClassChoicesReadMetrics;
}

RepositoryStatus TestingClassRepository::deleteTestingClass(
    int classId
    )
{
    const RepositoryStatus deleted = deleteTestingClassDetailed(classId);
    if (!deleted)
    {
        return std::unexpected(deleted.error());
    }

    return {};
}

RepositoryStatus
TestingClassRepository::deleteTestingClassDetailed(
    int classId
    )
{
    const auto technicalFailure = [](const QString& diagnostic)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            diagnostic
        });
    };
    const auto missingFailure = []
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr("The testing class no longer exists.")
        });
    };

    if (classId <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr("The testing class no longer exists.")
        });
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return technicalFailure(
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
        }
    };

    for (const auto& deletion : deletions)
    {
        query.prepare(deletion.first);
        query.addBindValue(classId);
        if (!query.exec())
        {
            return std::unexpected(queryFailure(query, deletion.second));
        }
    }

    query.prepare(QStringLiteral(R"(
        DELETE FROM classes
        WHERE id=?
        AND EXISTS (
            SELECT 1 FROM testing_classes WHERE class_id=?
        )
    )"));
    query.addBindValue(classId);
    query.addBindValue(classId);
    if (!query.exec())
    {
        return std::unexpected(queryFailure(
            query,
            QObject::tr("Removing the testing class")
            ));
    }

    const int affectedRows = query.numRowsAffected();
    if (affectedRows < 0)
    {
        return technicalFailure(
            QObject::tr(
                "Removing the testing class returned an unknown affected-row count."
                )
            );
    }
    if (affectedRows == 0)
    {
        if (!transaction.rollbackChecked())
        {
            return technicalFailure(
                QObject::tr(
                    "Rolling back the missing testing class deletion failed: %1"
                    ).arg(m_database.lastError().text())
                );
        }

        return missingFailure();
    }
    if (affectedRows != 1)
    {
        return technicalFailure(
            QObject::tr(
                "Removing the testing class affected an unexpected number of rows."
                )
            );
    }

    // Production profiles use ON DELETE CASCADE for this subtype row. The
    // direct delete also keeps compatibility with existing schemas that do
    // not declare that foreign key.
    query.prepare(QStringLiteral(
        "DELETE FROM testing_classes WHERE class_id=?"
        ));
    query.addBindValue(classId);
    if (!query.exec())
    {
        return std::unexpected(queryFailure(
            query,
            QObject::tr("Removing the testing class profile")
            ));
    }

    if (!transaction.commit())
    {
        return technicalFailure(
            QObject::tr("Committing the testing class deletion failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    return {};
}

RepositoryResult<bool> TestingClassRepository::isTestingClass(
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

    const bool exists = query.next();
    if (query.lastError().isValid())
    {
        return std::unexpected(queryFailure(
            query,
            QObject::tr("Checking the testing class")
            ));
    }

    return exists;
}
