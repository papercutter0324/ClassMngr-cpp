#include "data/data_service.h"

#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_transfer_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/teacher_service.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTime>
#include <QUuid>

#include <cstdio>
#include <memory>
#include <optional>

namespace
{
struct FixtureDefinition
{
    QString fileName;
};

struct FixtureVerification
{
    QString fileName;
    bool expectedToOpen;
    bool expectedBytesUnchanged;
    std::optional<int> expectedSchemaVersionAfterOpen;
};

const QList<FixtureDefinition> FixtureDefinitions{
    {QStringLiteral("empty.tps")},
    {QStringLiteral("typical.tps")},
    {QStringLiteral("roster-large.tps")},
    {QStringLiteral("large.tps")},
    {QStringLiteral("analytics-empty.tps")},
    {QStringLiteral("legacy-v2.db")},
    {QStringLiteral("legacy-v5.db")},
    {QStringLiteral("migration-invalid.db")},
    {QStringLiteral("migration-rollback.db")},
    {QStringLiteral("newer-schema.tps")},
    {QStringLiteral("corrupt.tps")}
};

QString connectionName()
{
    return QStringLiteral("database-port-fixture-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString fixtureKoreanName(int row)
{
    static const QStringList names{
        QStringLiteral("김민준"),
        QStringLiteral("이서연"),
        QStringLiteral("박지호"),
        QStringLiteral("최수빈"),
        QStringLiteral("정도윤"),
        QStringLiteral("강하은"),
        QStringLiteral("조현우"),
        QStringLiteral("윤서아"),
        QStringLiteral("장예준"),
        QStringLiteral("한유진")
    };

    return names.at(row % names.size());
}

QString fixtureEnglishName(int row)
{
    QString suffix;
    int value = row + 1;
    while (value > 0)
    {
        --value;
        suffix.prepend(QChar('A' + (value % 26)));
        value /= 26;
    }

    return QStringLiteral("Student %1").arg(suffix);
}

bool requireStatus(const Status& status, QString* error)
{
    if (status)
    {
        return true;
    }

    *error = status.error();
    return false;
}

bool ensureOutputPathsAreAvailable(const QString& outputDirectory, QString* error)
{
    QDir directory(outputDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
    {
        *error = QStringLiteral("Unable to create fixture directory: %1").arg(outputDirectory);
        return false;
    }

    for (const FixtureDefinition& fixture : FixtureDefinitions)
    {
        const QString filePath = directory.filePath(fixture.fileName);
        if (QFileInfo::exists(filePath))
        {
            *error = QStringLiteral("Refusing to overwrite existing fixture: %1").arg(filePath);
            return false;
        }
    }

    return true;
}

bool createProfile(
    const QString& filePath,
    int rosterRowCount,
    QString* error,
    bool includeSpeakingEvaluation = true
    )
{
    DataService service;
    if (!requireStatus(service.openDatabase(filePath), error))
    {
        return false;
    }

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Fixture Teacher");
    teacher.teacherKr = QStringLiteral("대표 교사");
    teacher.preferredRomanization = QStringLiteral("Daepyo Gyosa");
    teacher.preferredName = QStringLiteral("Fixture");
    teacher.roomNumber = QStringLiteral("301");
    teacher.birthday = QStringLiteral("02-29");
    teacher.phoneNumber = QStringLiteral("010-0000-0000");
    teacher.notes = QStringLiteral("English and 한국어 fixture data");

    const Result<int> teacherId = service.createTeacher(teacher);
    if (!teacherId)
    {
        *error = teacherId.error();
        return false;
    }

    const Result<int> classId = service.createClass(
        QStringLiteral("Fixture Class / 수업")
        );
    if (!classId)
    {
        *error = classId.error();
        return false;
    }

    ClassInfo classInfo;
    classInfo.classId = *classId;
    classInfo.teacherId = *teacherId;
    classInfo.classGrade = QStringLiteral("E6");
    classInfo.classLevel = QStringLiteral("Helios");
    classInfo.readingBook = QStringLiteral("Reading Explorer 3");
    classInfo.essayBook = QStringLiteral("6A");
    classInfo.notes = QStringLiteral("Fixture class notes / 수업 메모");
    classInfo.classTimes.append({
        QStringLiteral("Tuesday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")
        });
    if (!requireStatus(service.saveClassInfo(classInfo), error))
    {
        return false;
    }

    CalendarEvent event;
    event.title = QStringLiteral("Fixture Event / 행사");
    event.startDate = QDate(2026, 7, 17);
    event.endDate = event.startDate;
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    if (!service.saveCalendarEvent(event))
    {
        *error = QStringLiteral("Unable to save fixture calendar event.");
        return false;
    }

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows.reserve(rosterRowCount);
    for (int row = 0; row < rosterRowCount; ++row)
    {
        roster.rows.append({
            fixtureEnglishName(row),
            fixtureKoreanName(row)
            });
    }
    if (!requireStatus(service.saveRoster(*classId, roster), error))
    {
        return false;
    }

    if (includeSpeakingEvaluation)
    {
        const QStringList scores{
            QStringLiteral("A+"),
            QStringLiteral("A"),
            QStringLiteral("B+"),
            QStringLiteral("B"),
            QStringLiteral("C")
        };
        SpeakingEvalRows evaluation = SpeakingEval::emptyRows();
        const int evaluatedRowCount =
            qMin(rosterRowCount, SpeakingEval::RowCount);
        for (int row = 0; row < evaluatedRowCount; ++row)
        {
            evaluation[row][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
                fixtureEnglishName(row);
            evaluation[row][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
                fixtureKoreanName(row);

            const QString score = scores.at(row % scores.size());
            for (const SpeakingEvalColumn column : {
                     SpeakingEvalColumn::Grammar,
                     SpeakingEvalColumn::Pronunciation,
                     SpeakingEvalColumn::Fluency,
                     SpeakingEvalColumn::Manner,
                     SpeakingEvalColumn::Content,
                     SpeakingEvalColumn::OverallEffort
                 })
            {
                evaluation[row][SpeakingEval::toInt(column)] = score;
            }
        }

        if (!requireStatus(
                service.saveSpeakingEval(
                    *classId,
                    QStringLiteral("Winter"),
                    evaluation
                    ),
                error
                ))
        {
            return false;
        }
    }

    CampusRecord campus;
    campus.name = QStringLiteral("Fixture Campus");
    campus.address = QStringLiteral("Seoul / 서울");
    campus.officeNumber = QStringLiteral("02-0000-0000");
    if (!service.saveCampus(campus))
    {
        *error = QStringLiteral("Unable to save fixture campus.");
        return false;
    }

    service.closeDatabase();
    return true;
}

bool createSqlFixture(
    const QString& filePath,
    const QStringList& statements,
    QString* error
    );

bool createEmptyProfile(const QString& filePath, QString* error)
{
    DataService service;
    if (!requireStatus(service.openDatabase(filePath), error))
    {
        return false;
    }

    service.closeDatabase();
    return true;
}

bool createLegacyV2Profile(const QString& filePath, QString* error)
{
    if (!createProfile(filePath, 1, error))
    {
        return false;
    }

    return createSqlFixture(
        filePath,
        {
            QStringLiteral("PRAGMA foreign_keys = OFF"),
            QStringLiteral("UPDATE class_info SET teacher_id = -1"),
            QStringLiteral("PRAGMA user_version = 2")
        },
        error
        );
}

bool createSqlFixture(
    const QString& filePath,
    const QStringList& statements,
    QString* error
    )
{
    error->clear();
    const QString name = connectionName();
    bool succeeded = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(filePath);
        if (!database.open())
        {
            *error = database.lastError().text();
        }
        else
        {
            QSqlQuery query(database);
            for (const QString& statement : statements)
            {
                if (!query.exec(statement))
                {
                    *error = query.lastError().text();
                    break;
                }
            }
            succeeded = error->isEmpty();
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return succeeded;
}

bool createCorruptFixture(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        *error = file.errorString();
        return false;
    }

    if (file.write(QByteArrayLiteral("not a SQLite database")) < 0)
    {
        *error = file.errorString();
        return false;
    }

    return true;
}

bool createFixtures(const QString& outputDirectory, QString* error)
{
    if (!ensureOutputPathsAreAvailable(outputDirectory, error))
    {
        return false;
    }

    const QDir directory(outputDirectory);
    if (!createEmptyProfile(directory.filePath(QStringLiteral("empty.tps")), error)
        || !createProfile(directory.filePath(QStringLiteral("typical.tps")), 2, error)
        || !createProfile(directory.filePath(QStringLiteral("roster-large.tps")), 25, error)
        || !createProfile(directory.filePath(QStringLiteral("large.tps")), 1'000, error))
    {
        return false;
    }

    if (!createProfile(
            directory.filePath(QStringLiteral("analytics-empty.tps")),
            2,
            error,
            false
            ))
    {
        return false;
    }

    if (!createLegacyV2Profile(
            directory.filePath(QStringLiteral("legacy-v2.db")),
            error
            ))
    {
        return false;
    }

    if (!createProfile(directory.filePath(QStringLiteral("legacy-v5.db")), 1, error)
        || !createSqlFixture(
            directory.filePath(QStringLiteral("migration-invalid.db")),
            {
                QStringLiteral("CREATE TABLE teachers (id INTEGER PRIMARY KEY AUTOINCREMENT, teacher_kr TEXT, teacher_en TEXT, preferred_romanization TEXT, preferred_name TEXT, room_number TEXT, birthday TEXT, phone_number TEXT, wifi_name TEXT, wifi_password TEXT, internet_type TEXT DEFAULT 'WiFi', zoom_id TEXT, zoom_password TEXT, projection_type TEXT DEFAULT 'HDMI', notes TEXT)"),
                QStringLiteral("INSERT INTO teachers (teacher_en, internet_type, projection_type) VALUES ('Invalid Fixture Teacher', 'Satellite', 'HDMI')"),
                QStringLiteral("PRAGMA user_version = 2")
            },
            error
            )
        || !createSqlFixture(
            directory.filePath(QStringLiteral("migration-rollback.db")),
            {
                QStringLiteral("CREATE TABLE teachers (id INTEGER PRIMARY KEY AUTOINCREMENT, teacher_kr TEXT, teacher_en TEXT, preferred_romanization TEXT, preferred_name TEXT, room_number TEXT, birthday TEXT, phone_number TEXT, wifi_name TEXT, wifi_password TEXT, internet_type TEXT DEFAULT 'WiFi', zoom_id TEXT, zoom_password TEXT, projection_type TEXT DEFAULT 'HDMI', notes TEXT)"),
                QStringLiteral("CREATE TABLE classes (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)"),
                QStringLiteral("CREATE TABLE classmngr_legacy_classes (id INTEGER PRIMARY KEY)"),
                QStringLiteral("INSERT INTO teachers (teacher_en) VALUES ('Rollback Fixture Teacher')")
            },
            error
            )
        || !createSqlFixture(
            directory.filePath(QStringLiteral("newer-schema.tps")),
            {QStringLiteral("PRAGMA user_version = 7")},
            error
            )
        || !createCorruptFixture(
            directory.filePath(QStringLiteral("corrupt.tps")),
            error
            ))
    {
        return false;
    }

    const QString legacyV5Path = directory.filePath(QStringLiteral("legacy-v5.db"));
    return createSqlFixture(
        legacyV5Path,
        {QStringLiteral("PRAGMA user_version = 5")},
        error
        );
}

bool requireSqlValue(
    QSqlDatabase& database,
    const QString& statement,
    const QString& expectedValue,
    const QString& fixtureName,
    QString* error
    )
{
    QSqlQuery query(database);
    if (!query.exec(statement) || !query.next())
    {
        *error = QStringLiteral("Unable to verify %1: %2")
            .arg(fixtureName, query.lastError().text());
        return false;
    }

    const QString actualValue = query.value(0).toString();
    if (actualValue != expectedValue)
    {
        *error = QStringLiteral(
            "Unexpected semantic value for %1: expected '%2', got '%3'"
            ).arg(fixtureName, expectedValue, actualValue);
        return false;
    }

    return true;
}

bool verifyFixtureSemantics(
    const QString& copiedFixturePath,
    const QString& fixtureName,
    bool expectedToOpen,
    QString* error
    )
{
    const bool isRollbackFixture = fixtureName == QStringLiteral("migration-rollback.db");
    if (!expectedToOpen && !isRollbackFixture)
    {
        return true;
    }

    const QString name = connectionName();
    bool verified = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(copiedFixturePath);
        if (!database.open())
        {
            *error = QStringLiteral("Unable to inspect fixture semantics: %1")
                .arg(copiedFixturePath);
        }
        else if (fixtureName == QStringLiteral("empty.tps"))
        {
            verified = requireSqlValue(
                database,
                QStringLiteral("PRAGMA user_version"),
                QStringLiteral("6"),
                fixtureName,
                error
                )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT COUNT(*) FROM teachers"),
                    QStringLiteral("0"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT COUNT(*) FROM classes"),
                    QStringLiteral("0"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT COUNT(*) FROM calendar_events"),
                    QStringLiteral("0"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT COUNT(*) FROM campuses"),
                    QStringLiteral("0"),
                    fixtureName,
                    error
                    );
        }
        else if (fixtureName == QStringLiteral("typical.tps"))
        {
            verified = requireSqlValue(
                database,
                QStringLiteral("PRAGMA user_version"),
                QStringLiteral("6"),
                fixtureName,
                error
                )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT teacher_en || '|' || teacher_kr FROM teachers WHERE id=1"
                        ),
                    QStringLiteral("Fixture Teacher|대표 교사"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT name FROM classes WHERE id=1"),
                    QStringLiteral("Fixture Class / 수업"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT COUNT(DISTINCT row_index) FROM roster_data WHERE class_id=1"
                        ),
                    QStringLiteral("2"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT title FROM calendar_events WHERE id=1"),
                    QStringLiteral("Fixture Event / 행사"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT name FROM campuses WHERE id=1"),
                    QStringLiteral("Fixture Campus"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT evaluation_name FROM speaking_evaluations WHERE class_id=1"
                        ),
                    QStringLiteral("Winter"),
                    fixtureName,
                    error
                    );
        }
        else if (fixtureName == QStringLiteral("large.tps"))
        {
            verified = requireSqlValue(
                database,
                QStringLiteral("PRAGMA user_version"),
                QStringLiteral("6"),
                fixtureName,
                error
                )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT COUNT(DISTINCT row_index) FROM roster_data WHERE class_id=1"
                        ),
                    QStringLiteral("1000"),
                    fixtureName,
                    error
                    );
        }
        else if (fixtureName == QStringLiteral("roster-large.tps"))
        {
            verified = requireSqlValue(
                database,
                QStringLiteral("PRAGMA user_version"),
                QStringLiteral("6"),
                fixtureName,
                error
                )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT COUNT(DISTINCT row_index) FROM roster_data WHERE class_id=1"
                        ),
                    QStringLiteral("25"),
                    fixtureName,
                    error
                    );
        }
        else if (fixtureName == QStringLiteral("analytics-empty.tps"))
        {
            verified = requireSqlValue(
                database,
                QStringLiteral("PRAGMA user_version"),
                QStringLiteral("6"),
                fixtureName,
                error
                )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT name FROM classes WHERE id=1"),
                    QStringLiteral("Fixture Class / 수업"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT COUNT(DISTINCT row_index) FROM roster_data WHERE class_id=1"
                        ),
                    QStringLiteral("2"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT COUNT(*) FROM speaking_evaluations WHERE class_id=1"
                        ),
                    QStringLiteral("0"),
                    fixtureName,
                    error
                    );
        }
        else if (fixtureName == QStringLiteral("legacy-v2.db"))
        {
            verified = requireSqlValue(
                database,
                QStringLiteral("PRAGMA user_version"),
                QStringLiteral("6"),
                fixtureName,
                error
                )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT CASE WHEN teacher_id IS NULL THEN 1 ELSE 0 END "
                        "FROM class_info WHERE class_id=1"
                        ),
                    QStringLiteral("1"),
                    fixtureName,
                    error
                    );
        }
        else if (fixtureName == QStringLiteral("legacy-v5.db"))
        {
            verified = requireSqlValue(
                database,
                QStringLiteral("PRAGMA user_version"),
                QStringLiteral("6"),
                fixtureName,
                error
                );
        }
        else if (isRollbackFixture)
        {
            verified = requireSqlValue(
                database,
                QStringLiteral("PRAGMA user_version"),
                QStringLiteral("3"),
                fixtureName,
                error
                )
                && requireSqlValue(
                    database,
                    QStringLiteral(
                        "SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='classmngr_legacy_classes'"
                        ),
                    QStringLiteral("1"),
                    fixtureName,
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT teacher_en FROM teachers WHERE id=1"),
                    QStringLiteral("Rollback Fixture Teacher"),
                    fixtureName,
                    error
                    );
        }
        else
        {
            *error = QStringLiteral("No semantic verifier for fixture: %1").arg(fixtureName);
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);

    if (!verified)
    {
        return false;
    }

    if (fixtureName == QStringLiteral("legacy-v2.db")
        && !QFileInfo::exists(copiedFixturePath + QStringLiteral(".pre-schema-v4-backup")))
    {
        *error = QStringLiteral("Legacy v2 fixture did not create its v4 backup.");
        return false;
    }
    if (fixtureName == QStringLiteral("legacy-v5.db")
        && !QFileInfo::exists(copiedFixturePath + QStringLiteral(".pre-schema-v6-backup")))
    {
        *error = QStringLiteral("Legacy v5 fixture did not create its v6 backup.");
        return false;
    }
    if (isRollbackFixture
        && !QFileInfo::exists(copiedFixturePath + QStringLiteral(".pre-schema-v4-backup")))
    {
        *error = QStringLiteral("Rollback fixture did not create its v4 backup.");
        return false;
    }

    return true;
}

bool verifyFixtureOpens(
    const QString& fixturePath,
    const QString& fixtureName,
    bool expectedToOpen,
    bool expectedBytesUnchanged,
    std::optional<int> expectedSchemaVersionAfterOpen,
    QString* error
    )
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        *error = QStringLiteral("Unable to create fixture verification directory.");
        return false;
    }

    const QString copiedFixturePath = QDir(temporaryDirectory.path()).filePath(
        QFileInfo(fixturePath).fileName()
        );
    if (!QFile::copy(fixturePath, copiedFixturePath))
    {
        *error = QStringLiteral("Unable to copy fixture for verification: %1").arg(fixturePath);
        return false;
    }
    QFile copiedFixture(copiedFixturePath);
    if (!copiedFixture.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("Unable to read fixture before verification: %1")
            .arg(copiedFixturePath);
        return false;
    }
    const QByteArray bytesBeforeOpen = copiedFixture.readAll();
    copiedFixture.close();

    DataService service;
    const Status opened = service.openDatabase(copiedFixturePath);
    const bool openedSuccessfully = opened.has_value();
    if (openedSuccessfully != expectedToOpen)
    {
        *error = expectedToOpen
            ? QStringLiteral("Fixture did not open: %1").arg(opened.error())
            : QStringLiteral("Fixture unexpectedly opened: %1").arg(fixturePath);
        return false;
    }

    service.closeDatabase();

    if (!expectedToOpen && expectedBytesUnchanged)
    {
        if (!copiedFixture.open(QIODevice::ReadOnly))
        {
            *error = QStringLiteral("Unable to read fixture after failed open: %1")
                .arg(copiedFixturePath);
            return false;
        }
        const QByteArray bytesAfterOpen = copiedFixture.readAll();
        copiedFixture.close();
        if (bytesAfterOpen != bytesBeforeOpen)
        {
            *error = QStringLiteral("Failed fixture was mutated: %1").arg(fixturePath);
            return false;
        }
    }

    if (expectedSchemaVersionAfterOpen.has_value())
    {
        const QString name = connectionName();
        int actualSchemaVersion = -1;
        bool schemaVersionRead = false;
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                name
                );
            database.setDatabaseName(copiedFixturePath);
            if (!database.open())
            {
                *error = QStringLiteral("Unable to inspect fixture schema version: %1")
                    .arg(copiedFixturePath);
            }
            else
            {
                QSqlQuery query(database);
                if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
                {
                    *error = QStringLiteral("Unable to read fixture schema version: %1")
                        .arg(copiedFixturePath);
                }
                else
                {
                    actualSchemaVersion = query.value(0).toInt();
                    schemaVersionRead = true;
                }
                database.close();
            }
        }
        QSqlDatabase::removeDatabase(name);

        if (!schemaVersionRead)
        {
            return false;
        }
        if (actualSchemaVersion != *expectedSchemaVersionAfterOpen)
        {
            *error = QStringLiteral(
                "Fixture schema version after open was %1, expected %2: %3"
                ).arg(actualSchemaVersion)
                    .arg(*expectedSchemaVersionAfterOpen)
                    .arg(fixturePath);
            return false;
        }
    }

    if (!verifyFixtureSemantics(
            copiedFixturePath,
            fixtureName,
            expectedToOpen,
            error
            ))
    {
        return false;
    }

    return true;
}

bool verifyFixtures(const QString& fixtureDirectory, QString* error)
{
    const QDir directory(fixtureDirectory);
    const QList<FixtureVerification> expectations{
        {QStringLiteral("empty.tps"), true, false, std::nullopt},
        {QStringLiteral("typical.tps"), true, false, std::nullopt},
        {QStringLiteral("roster-large.tps"), true, false, std::nullopt},
        {QStringLiteral("large.tps"), true, false, std::nullopt},
        {QStringLiteral("analytics-empty.tps"), true, false, std::nullopt},
        {QStringLiteral("legacy-v2.db"), true, false, std::nullopt},
        {QStringLiteral("legacy-v5.db"), true, false, std::nullopt},
        {QStringLiteral("migration-invalid.db"), false, true, std::nullopt},
        {QStringLiteral("migration-rollback.db"), false, false, 3},
        {QStringLiteral("newer-schema.tps"), false, true, std::nullopt},
        {QStringLiteral("corrupt.tps"), false, true, std::nullopt}
    };

    for (const FixtureVerification& expectation : expectations)
    {
        const QString fixturePath = directory.filePath(expectation.fileName);
        if (!QFileInfo(fixturePath).isFile())
        {
            *error = QStringLiteral("Required fixture is missing: %1").arg(fixturePath);
            return false;
        }
        if (!verifyFixtureOpens(
                fixturePath,
                expectation.fileName,
                expectation.expectedToOpen,
                expectation.expectedBytesUnchanged,
                expectation.expectedSchemaVersionAfterOpen,
                error
                ))
        {
            return false;
        }
    }

    return true;
}

bool verifyQtWrittenProfileWithEngine(QString* error)
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        *error = QStringLiteral(
            "Unable to create a temporary directory for the Qt-to-engine round trip."
            );
        return false;
    }

    const QString path = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("qt-written.tps")
        );
    if (!createProfile(path, 2, error))
    {
        return false;
    }

    auto opened = classmngr::engine::OpenDatabase::execute(
        path.toStdString()
        );
    if (!opened || *opened == nullptr)
    {
        *error = QStringLiteral(
            "The engine could not open a profile written by the Qt DataService: %1"
            ).arg(
                opened
                    ? QStringLiteral("no database handle")
                    : QString::fromStdString(opened.error().message)
                );
        return false;
    }

    std::unique_ptr<classmngr::engine::SqliteDatabase> database =
        std::move(*opened);
    classmngr::engine::ClassTransferService transfers(*database);
    const auto package = transfers.buildPackage({1});
    if (!package
        || package->teachers.size() != 1
        || package->classes.size() != 1
        || package->classes.front().name != "Fixture Class / 수업"
        || package->classes.front().roster.rows.size() != 2
        || package->classes.front().evaluations.size() != 1)
    {
        *error = QStringLiteral(
            "The engine did not recover the complete Qt-written profile payload."
            );
        return false;
    }

    return true;
}

bool createEngineWrittenProfile(
    const QString& path,
    QString* error
    )
{
    auto opened = classmngr::engine::OpenDatabase::execute(
        path.toStdString()
        );
    if (!opened || *opened == nullptr)
    {
        *error = QStringLiteral(
            "The engine could not create its round-trip profile: %1"
            ).arg(
                opened
                    ? QStringLiteral("no database handle")
                    : QString::fromStdString(opened.error().message)
                );
        return false;
    }

    auto database = std::move(*opened);
    classmngr::engine::Teacher teacher;
    teacher.teacherEn = "Engine Teacher";
    teacher.teacherKr = "\xEC\x97\x94\xEC\xA7\x84 \xEA\xB5\x90\xEC\x82\xAC";
    teacher.preferredRomanization = "Engine Teacher";
    teacher.preferredName = teacher.teacherEn;
    teacher.roomNumber = "302";
    teacher.birthday = "03-07";
    teacher.phoneNumber = "010-1111-2222";

    classmngr::engine::TeacherService teachers(*database);
    const auto teacherId = teachers.create(teacher);
    if (!teacherId)
    {
        *error = QString::fromStdString(teacherId.error().message);
        return false;
    }

    classmngr::engine::ClassRepository classes(*database);
    const auto classId = classes.create(
        "Engine Class / \xEC\x88\x98\xEC\x97\x85"
        );
    if (!classId)
    {
        *error = QString::fromStdString(classId.error().message);
        return false;
    }

    classmngr::engine::ClassInfo info;
    info.classId = *classId;
    info.teacherId = *teacherId;
    info.classGrade = "E6";
    info.classLevel = "Helios";
    info.readingBook = "Reading Explorer 3";
    info.essayBook = "6A";
    info.classTimes = {{"Tuesday", "16:00", "16:50"}};

    classmngr::engine::ClassInfoService classInfo(*database);
    if (const auto saved = classInfo.save(info); !saved)
    {
        *error = QString::fromStdString(saved.error().message);
        return false;
    }

    database.reset();
    return true;
}

bool verifyEngineWrittenProfileWithQt(QString* error)
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        *error = QStringLiteral(
            "Unable to create a temporary directory for the engine-to-Qt round trip."
            );
        return false;
    }

    const QString path = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("engine-written.tps")
        );
    if (!createEngineWrittenProfile(path, error))
    {
        return false;
    }

    const QString name = connectionName();
    bool verified = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            name
            );
        database.setDatabaseName(path);
        if (!database.open())
        {
            *error = QStringLiteral(
                "Qt could not open the profile written by the engine: %1"
                ).arg(database.lastError().text());
        }
        else
        {
            verified = requireSqlValue(
                database,
                QStringLiteral(
                    "SELECT teacher_en || '|' || teacher_kr FROM teachers WHERE id=1"
                    ),
                QStringLiteral("Engine Teacher|엔진교사"),
                QStringLiteral("engine-written.tps"),
                error
                )
                && requireSqlValue(
                    database,
                    QStringLiteral("SELECT name FROM classes WHERE id=1"),
                    QStringLiteral("Engine Class / 수업"),
                    QStringLiteral("engine-written.tps"),
                    error
                    )
                && requireSqlValue(
                    database,
                    QStringLiteral("PRAGMA user_version"),
                    QStringLiteral("6"),
                    QStringLiteral("engine-written.tps"),
                    error
                    );
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return verified;
}

bool verifyCrossPlatformRoundTrips(QString* error)
{
    return verifyQtWrittenProfileWithEngine(error)
        && verifyEngineWrittenProfileWithQt(error);
}

void printUsage(const QString& executable)
{
    qInfo().noquote()
        << QStringLiteral(
               "Usage: %1 --output-directory <directory> | --verify-directory <directory>"
               ).arg(executable);
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    const int outputIndex = arguments.indexOf(QStringLiteral("--output-directory"));
    const int verifyIndex = arguments.indexOf(QStringLiteral("--verify-directory"));
    const bool hasOutputDirectory = outputIndex >= 0 && outputIndex + 1 < arguments.size();
    const bool hasVerifyDirectory = verifyIndex >= 0 && verifyIndex + 1 < arguments.size();
    if (hasOutputDirectory == hasVerifyDirectory)
    {
        printUsage(arguments.constFirst());
        return 2;
    }

    QString error;
    const QString directory = hasOutputDirectory
        ? arguments.at(outputIndex + 1)
        : arguments.at(verifyIndex + 1);
    bool succeeded = hasOutputDirectory
        ? createFixtures(directory, &error)
        : verifyFixtures(directory, &error);
    if (succeeded && hasVerifyDirectory)
    {
        succeeded = verifyCrossPlatformRoundTrips(&error);
    }
    if (!succeeded)
    {
        qCritical().noquote() << error;
        std::fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }

    qInfo().noquote()
        << QStringLiteral("%1 Phase 0 database fixtures in %2")
            .arg(hasOutputDirectory ? QStringLiteral("Created") : QStringLiteral("Verified"))
            .arg(directory);
    return 0;
}

