#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_transfer_service.h"
#include "classmngr/engine/database_schema.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/teacher_service.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifndef CLASSMNGR_DATABASE_PORT_FIXTURE_DIR
#error "CLASSMNGR_DATABASE_PORT_FIXTURE_DIR must be defined for this test."
#endif

namespace
{
namespace Engine = classmngr::engine;
namespace Filesystem = std::filesystem;

struct TemporaryDirectory final
{
    Filesystem::path path;

    ~TemporaryDirectory()
    {
        std::error_code error;
        Filesystem::remove_all(path, error);
    }
};

struct CurrentFixture
{
    std::string_view fileName;
    int rosterRows;
    bool hasSpeakingEvaluation;
};

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineDatabaseFixtureRoundTripTests: "
              << message
              << '\n';
    return false;
}

std::string pathToUtf8(
    const Filesystem::path& path
    )
{
    const std::u8string encoded = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size()
        );
}

Filesystem::path pathFromUtf8(
    std::string_view path
    )
{
    return Filesystem::u8path(path.begin(), path.end());
}

std::optional<std::string> readBytes(
    const Filesystem::path& path
    )
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return std::nullopt;
    }

    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
        );
}

std::optional<TemporaryDirectory> makeTemporaryDirectory()
{
    std::error_code error;
    const Filesystem::path root = Filesystem::temp_directory_path(error);
    if (error)
    {
        return std::nullopt;
    }

    const auto suffix = std::chrono::steady_clock::now()
        .time_since_epoch()
        .count();
    TemporaryDirectory result{
        root / ("classmngr-engine-database-fixtures-"
            + std::to_string(suffix))
    };
    Filesystem::create_directories(result.path, error);
    if (error)
    {
        return std::nullopt;
    }
    return result;
}

bool copyFixture(
    const Filesystem::path& fixtureRoot,
    const Filesystem::path& temporaryRoot,
    std::string_view fileName,
    Filesystem::path* destination
    )
{
    const Filesystem::path source = fixtureRoot / pathFromUtf8(fileName);
    *destination = temporaryRoot / pathFromUtf8(fileName);

    std::error_code error;
    Filesystem::create_directories(temporaryRoot, error);
    if (error)
    {
        return false;
    }
    return Filesystem::is_regular_file(source, error)
        && !error
        && Filesystem::copy_file(
            source,
            *destination,
            Filesystem::copy_options::overwrite_existing,
            error
            )
        && !error;
}

bool sameTimes(
    const std::vector<Engine::ClassTime>& lhs,
    const std::vector<Engine::ClassTime>& rhs
    )
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        if (lhs[index].day != rhs[index].day
            || lhs[index].startTime != rhs[index].startTime
            || lhs[index].endTime != rhs[index].endTime)
        {
            return false;
        }
    }
    return true;
}

std::optional<std::int64_t> queryInteger(
    Engine::SqliteDatabase& database,
    std::string_view sql
    )
{
    const auto result = database.query(sql);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return std::nullopt;
    }

    const auto* value = std::get_if<std::int64_t>(
        &result->rows.front().values.front()
        );
    return value == nullptr
        ? std::nullopt
        : std::optional<std::int64_t>{*value};
}

std::optional<std::string> queryText(
    Engine::SqliteDatabase& database,
    std::string_view sql
    )
{
    const auto result = database.query(sql);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return std::nullopt;
    }

    const auto* value = std::get_if<std::string>(
        &result->rows.front().values.front()
        );
    return value == nullptr
        ? std::nullopt
        : std::optional<std::string>{*value};
}

bool isNull(
    Engine::SqliteDatabase& database,
    std::string_view sql
    )
{
    const auto result = database.query(sql);
    return result && result->rows.size() == 1
        && result->rows.front().values.size() == 1
        && std::holds_alternative<std::monostate>(
            result->rows.front().values.front()
            );
}

bool sameTeacherContent(
    const Engine::Teacher& lhs,
    const Engine::Teacher& rhs
    )
{
    return lhs.teacherKr == rhs.teacherKr
        && lhs.teacherEn == rhs.teacherEn
        && lhs.preferredRomanization == rhs.preferredRomanization
        && lhs.preferredName == rhs.preferredName
        && lhs.roomNumber == rhs.roomNumber
        && lhs.birthday == rhs.birthday
        && lhs.phoneNumber == rhs.phoneNumber
        && lhs.wifiName == rhs.wifiName
        && lhs.wifiPassword == rhs.wifiPassword
        && lhs.internetType == rhs.internetType
        && lhs.zoomId == rhs.zoomId
        && lhs.zoomPassword == rhs.zoomPassword
        && lhs.projectionType == rhs.projectionType
        && lhs.notes == rhs.notes;
}

bool sameClassInfoContent(
    const Engine::ClassInfo& lhs,
    const Engine::ClassInfo& rhs
    )
{
    return lhs.teacherKr == rhs.teacherKr
        && lhs.teacherEn == rhs.teacherEn
        && lhs.teacherPreferredName == rhs.teacherPreferredName
        && lhs.roomNumber == rhs.roomNumber
        && lhs.wifiName == rhs.wifiName
        && lhs.wifiPassword == rhs.wifiPassword
        && lhs.internetType == rhs.internetType
        && lhs.zoomId == rhs.zoomId
        && lhs.zoomPassword == rhs.zoomPassword
        && lhs.projectionType == rhs.projectionType
        && lhs.classGrade == rhs.classGrade
        && lhs.classLevel == rhs.classLevel
        && lhs.readingBook == rhs.readingBook
        && lhs.essayBook == rhs.essayBook
        && lhs.classColor == rhs.classColor
        && lhs.fontColor == rhs.fontColor
        && sameTimes(lhs.classTimes, rhs.classTimes)
        && sameTimes(lhs.intensiveTimes, rhs.intensiveTimes)
        && lhs.notes == rhs.notes
        && lhs.timeFillerActivities == rhs.timeFillerActivities;
}

bool sameClassContent(
    const Engine::ClassTransferClass& lhs,
    const Engine::ClassTransferClass& rhs
    )
{
    if (lhs.key != rhs.key
        || lhs.name != rhs.name
        || lhs.teacherKey != rhs.teacherKey
        || !sameClassInfoContent(lhs.info, rhs.info)
        || lhs.roster.columns != rhs.roster.columns
        || lhs.roster.columnWidths != rhs.roster.columnWidths
        || lhs.roster.rows != rhs.roster.rows
        || lhs.evaluations.size() != rhs.evaluations.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.evaluations.size(); ++index)
    {
        if (lhs.evaluations[index].name != rhs.evaluations[index].name
            || lhs.evaluations[index].rows != rhs.evaluations[index].rows)
        {
            return false;
        }
    }
    return true;
}

bool samePackageContent(
    const Engine::ClassTransferPackage& lhs,
    const Engine::ClassTransferPackage& rhs
    )
{
    if (lhs.version != rhs.version
        || lhs.teachers.size() != rhs.teachers.size()
        || lhs.classes.size() != rhs.classes.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.teachers.size(); ++index)
    {
        if (lhs.teachers[index].key != rhs.teachers[index].key
            || !sameTeacherContent(
                lhs.teachers[index].teacher,
                rhs.teachers[index].teacher
                ))
        {
            return false;
        }
    }

    for (std::size_t index = 0; index < lhs.classes.size(); ++index)
    {
        if (!sameClassContent(lhs.classes[index], rhs.classes[index]))
        {
            return false;
        }
    }
    return true;
}

bool verifyCurrentFixture(
    const Filesystem::path& fixturePath,
    const CurrentFixture& fixture
    )
{
    bool passed = true;
    auto opened = Engine::OpenDatabase::execute(pathToUtf8(fixturePath));
    passed &= expect(
        opened && *opened != nullptr,
        std::string(fixture.fileName) + " did not open through OpenDatabase"
        );
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    auto database = std::move(*opened);
    passed &= expect(
        database->schemaVersion().value_or(-1)
            == Engine::DatabaseSchemaManager::LatestSchemaVersion,
        std::string(fixture.fileName) + " did not remain at schema v6"
        );

    if (fixture.fileName == "empty.tps")
    {
        passed &= expect(
            queryInteger(*database, "SELECT COUNT(*) FROM teachers")
                    .value_or(-1) == 0
                && queryInteger(*database, "SELECT COUNT(*) FROM classes")
                    .value_or(-1) == 0
                && queryInteger(
                    *database,
                    "SELECT COUNT(*) FROM calendar_events"
                    ).value_or(-1) == 0
                && queryInteger(*database, "SELECT COUNT(*) FROM campuses")
                    .value_or(-1) == 0,
            "empty.tps did not remain an empty profile"
            );
        return passed;
    }

    Engine::ClassTransferService transfers(*database);
    const auto package = transfers.buildPackage({1});
    passed &= expect(
        package && package->classes.size() == 1
            && package->teachers.size() == 1
            && package->classes.front().name == "Fixture Class / 수업"
            && package->classes.front().teacherKey == "teacher-1"
            && package->classes.front().roster.rows.size()
                == static_cast<std::size_t>(fixture.rosterRows)
            && package->classes.front().evaluations.size()
                == (fixture.hasSpeakingEvaluation ? 1U : 0U),
        std::string(fixture.fileName)
            + " did not expose its complete class transfer payload"
        );
    if (!package)
    {
        return false;
    }

    const auto& transferClass = package->classes.front();
    const auto& teacher = package->teachers.front().teacher;
    passed &= expect(
        teacher.teacherEn == "Fixture Teacher"
            && teacher.teacherKr == "대표 교사"
            && teacher.preferredRomanization == "Daepyo Gyosa"
            && teacher.preferredName == "Fixture"
            && teacher.notes == "English and 한국어 fixture data",
        std::string(fixture.fileName) + " lost bilingual teacher data"
        );
    passed &= expect(
        transferClass.info.classGrade == "E6"
            && transferClass.info.classLevel == "Helios"
            && transferClass.info.readingBook == "Reading Explorer 3"
            && transferClass.info.essayBook == "6A"
            && transferClass.info.classTimes.size() == 1
            && transferClass.info.classTimes.front().day == "Tuesday"
            && transferClass.info.classTimes.front().startTime == "4:00 PM"
            && transferClass.info.classTimes.front().endTime == "4:50 PM",
        std::string(fixture.fileName) + " lost class-information data"
        );

    if (fixture.fileName == "typical.tps")
    {
        passed &= expect(
            queryInteger(*database, "SELECT COUNT(*) FROM calendar_events")
                    .value_or(-1) == 1
                && queryText(
                    *database,
                    "SELECT title FROM calendar_events WHERE id=1"
                    ).value_or("") == "Fixture Event / 행사"
                && queryInteger(*database, "SELECT COUNT(*) FROM campuses")
                    .value_or(-1) == 1
                && queryText(
                    *database,
                    "SELECT name FROM campuses WHERE id=1"
                    ).value_or("") == "Fixture Campus",
            "typical.tps did not preserve calendar and campus data"
            );
    }

    return passed;
}

bool verifyTypicalRoundTrip(
    const Filesystem::path& fixturePath,
    const Filesystem::path& temporaryRoot
    )
{
    bool passed = true;
    auto opened = Engine::OpenDatabase::execute(pathToUtf8(fixturePath));
    passed &= expect(
        opened && *opened != nullptr,
        "typical round-trip fixture did not open"
        );
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    auto database = std::move(*opened);
    Engine::ClassTransferService transfers(*database);
    const auto package = transfers.buildPackage({1});
    passed &= expect(
        package && package->classes.size() == 1,
        "typical fixture package could not be exported"
        );
    if (!package)
    {
        return false;
    }

    Engine::TeacherService teachers(*database);
    Engine::ClassRepository classes(*database);
    Engine::ClassInfoService classInfo(*database);

    Engine::Teacher changedTeacher = package->teachers.front().teacher;
    // The committed fixture predates the current preferred-name choice
    // constraint.  Keep the fixture read compatible, then use a canonical
    // choice for the engine-owned write path.
    changedTeacher.preferredName = changedTeacher.teacherEn;
    changedTeacher.notes = "Engine round-trip note / 엔진";
    const auto teacherUpdate = teachers.update(changedTeacher);
    const auto classRename = classes.rename(1, "Fixture Class / 엔진 수정");
    const auto noteSave = classInfo.saveNotes(
        1,
        "Engine notes / 메모",
        "Engine filler"
        );
    passed &= expect(
        teacherUpdate && classRename && noteSave,
        "engine fixture write-back failed"
        );

    database.reset();
    auto reopened = Engine::OpenDatabase::execute(pathToUtf8(fixturePath));
    passed &= expect(
        reopened && *reopened != nullptr,
        "engine-written fixture could not be reopened"
        );
    if (!reopened || *reopened == nullptr)
    {
        return false;
    }

    auto reopenedDatabase = std::move(*reopened);
    Engine::TeacherService reopenedTeachers(*reopenedDatabase);
    Engine::ClassRepository reopenedClasses(*reopenedDatabase);
    Engine::ClassInfoService reopenedClassInfo(*reopenedDatabase);
    const auto persistedTeacher = reopenedTeachers.get(1);
    const auto persistedClass = reopenedClasses.get(1);
    const auto persistedInfo = reopenedClassInfo.load(1);
    passed &= expect(
        persistedTeacher && persistedTeacher->notes == "Engine round-trip note / 엔진"
            && persistedClass && persistedClass->name == "Fixture Class / 엔진 수정"
            && persistedInfo && persistedInfo->notes == "Engine notes / 메모"
                && persistedInfo->timeFillerActivities == "Engine filler",
        "engine fixture write-back did not survive a reopen"
        );
    reopenedDatabase.reset();

    const Filesystem::path importedPath =
        temporaryRoot / "typical-engine-imported.tps";
    auto importedDatabaseResult = Engine::OpenDatabase::execute(
        pathToUtf8(importedPath)
        );
    passed &= expect(
        importedDatabaseResult && *importedDatabaseResult != nullptr,
        "empty destination for fixture transfer could not be opened"
        );
    if (!importedDatabaseResult || *importedDatabaseResult == nullptr)
    {
        return false;
    }

    auto importedDatabase = std::move(*importedDatabaseResult);
    Engine::ClassTransferService importedTransfers(*importedDatabase);
    Engine::ClassImportPlan plan;
    plan.classes.push_back({0, Engine::ClassImportAction::Create, -1});
    for (const auto& transferTeacher : package->teachers)
    {
        plan.teachers.push_back({
            transferTeacher.key,
            Engine::TeacherImportAction::Create,
            -1
        });
    }

    Engine::ClassTransferPackage importPackage = *package;
    // The fixture also contains the legacy spaced Korean teacher name.  The
    // validated engine write path canonicalizes Korean names while retaining
    // the same semantic teacher.
    importPackage.teachers.front().teacher.teacherKr = "대표교사";
    importPackage.teachers.front().teacher.preferredName =
        importPackage.teachers.front().teacher.teacherEn;
    const auto imported = importedTransfers.importClasses(importPackage, plan);
    passed &= expect(
        imported && imported->createdClassIds.size() == 1,
        "portable fixture transfer import failed"
        );
    if (!imported || imported->createdClassIds.size() != 1)
    {
        return false;
    }

    const int importedClassId = imported->createdClassIds.front();
    importedDatabase.reset();
    auto importedReopen = Engine::OpenDatabase::execute(
        pathToUtf8(importedPath)
        );
    passed &= expect(
        importedReopen && *importedReopen != nullptr,
        "portable fixture transfer output could not be reopened"
        );
    if (!importedReopen || *importedReopen == nullptr)
    {
        return false;
    }

    auto importedReopenedDatabase = std::move(*importedReopen);
    Engine::ClassTransferService importedReopenedTransfers(
        *importedReopenedDatabase
        );
    const auto roundTrippedPackage = importedReopenedTransfers.buildPackage({
        importedClassId
    });
    passed &= expect(
        roundTrippedPackage
            && samePackageContent(importPackage, *roundTrippedPackage),
        "portable fixture transfer did not preserve complete package content"
        );
    return passed;
}

bool verifyMigratedFixture(
    const Filesystem::path& fixturePath,
    std::string_view fixtureName,
    std::string_view backupSuffix
    )
{
    auto opened = Engine::OpenDatabase::execute(pathToUtf8(fixturePath));
    bool passed = expect(
        opened && *opened != nullptr,
        std::string(fixtureName) + " did not migrate through OpenDatabase"
        );
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    auto database = std::move(*opened);
    passed &= expect(
        database->schemaVersion().value_or(-1)
            == Engine::DatabaseSchemaManager::LatestSchemaVersion,
        std::string(fixtureName) + " did not migrate to schema v6"
        );
    const Filesystem::path backupPath = pathFromUtf8(
        pathToUtf8(fixturePath) + std::string(backupSuffix)
        );
    passed &= expect(
        Filesystem::is_regular_file(backupPath),
        std::string(fixtureName) + " did not create its migration backup"
        );
    if (fixtureName == "legacy-v2.db")
    {
        passed &= expect(
            isNull(*database, "SELECT teacher_id FROM class_info WHERE class_id=1"),
            "legacy-v2.db did not repair its unassigned teacher"
            );
    }
    return passed;
}

bool verifyRejectedFixture(
    const Filesystem::path& fixturePath,
    std::string_view fixtureName,
    bool sourceMustRemainUnchanged,
    std::optional<int> expectedSchemaVersion,
    std::string_view expectedMessage
    )
{
    const auto before = readBytes(fixturePath);
    bool passed = expect(
        before.has_value(),
        std::string(fixtureName) + " could not be read before rejection"
        );

    const auto opened = Engine::OpenDatabase::execute(pathToUtf8(fixturePath));
    passed &= expect(
        !opened,
        std::string(fixtureName) + " was unexpectedly accepted"
        );
    if (opened)
    {
        return false;
    }
    const bool messageMatches = opened.error().message.find(expectedMessage)
        != std::string::npos;
    if (!messageMatches)
    {
        std::cerr << std::string(fixtureName) << " rejection: "
                  << opened.error().message
                  << '\n';
    }
    passed &= expect(
        messageMatches,
        std::string(fixtureName) + " returned an unexpected rejection message"
        );

    const auto after = readBytes(fixturePath);
    if (sourceMustRemainUnchanged)
    {
        passed &= expect(
            before.has_value() && after.has_value() && *before == *after,
            std::string(fixtureName) + " was mutated after rejection"
            );
    }

    if (expectedSchemaVersion.has_value())
    {
        Engine::SqliteDatabase database;
        passed &= expect(
            database.open(pathToUtf8(fixturePath)).has_value()
                && database.schemaVersion().value_or(-1)
                    == *expectedSchemaVersion,
            std::string(fixtureName)
                + " did not retain the expected post-failure schema"
            );
        if (fixtureName == "migration-rollback.db")
        {
            passed &= expect(
                queryText(
                    database,
                    "SELECT teacher_en FROM teachers WHERE id=1"
                    ).value_or("") == "Rollback Fixture Teacher"
                    && queryInteger(
                        database,
                        "SELECT COUNT(*) FROM classmngr_legacy_classes"
                        ).value_or(-1) == 0,
                "migration-rollback.db did not preserve source data"
                );
            const Filesystem::path backupPath = pathFromUtf8(
                pathToUtf8(fixturePath) + ".pre-schema-v4-backup"
                );
            passed &= expect(
                Filesystem::is_regular_file(backupPath),
                "migration-rollback.db did not create its migration backup"
                );
        }
    }
    return passed;
}
} // namespace

int main()
{
    const Filesystem::path fixtureRoot = pathFromUtf8(
        CLASSMNGR_DATABASE_PORT_FIXTURE_DIR
        );
    const auto temporaryDirectory = makeTemporaryDirectory();
    if (!temporaryDirectory)
    {
        std::cerr << "ClassMngrEngineDatabaseFixtureRoundTripTests: "
                  << "could not create a temporary directory\n";
        return 1;
    }

    bool passed = true;
    const std::array<CurrentFixture, 5> currentFixtures{
        CurrentFixture{"empty.tps", 0, false},
        CurrentFixture{"typical.tps", 2, true},
        CurrentFixture{"roster-large.tps", 25, true},
        CurrentFixture{"large.tps", 1000, true},
        CurrentFixture{"analytics-empty.tps", 2, false}
    };

    for (const CurrentFixture& fixture : currentFixtures)
    {
        Filesystem::path copy;
        passed &= expect(
            copyFixture(
                fixtureRoot,
                temporaryDirectory->path,
                fixture.fileName,
                &copy
                ),
            std::string("could not copy ") + std::string(fixture.fileName)
                + " for engine verification"
            );
        if (Filesystem::exists(copy))
        {
            passed &= verifyCurrentFixture(copy, fixture);
        }
    }

    Filesystem::path typicalRoundTrip;
    passed &= expect(
        copyFixture(
            fixtureRoot,
            temporaryDirectory->path / "round-trip",
            "typical.tps",
            &typicalRoundTrip
            ),
        "could not copy typical.tps for the write/read round trip"
        );
    if (Filesystem::exists(typicalRoundTrip))
    {
        passed &= verifyTypicalRoundTrip(
            typicalRoundTrip,
            temporaryDirectory->path
            );
    }

    const std::array<std::pair<std::string_view, std::string_view>, 2>
        migratedFixtures{
            std::pair{"legacy-v2.db", ".pre-schema-v4-backup"},
            std::pair{"legacy-v5.db", ".pre-schema-v6-backup"}
        };
    for (const auto& fixture : migratedFixtures)
    {
        Filesystem::path copy;
        passed &= expect(
            copyFixture(
                fixtureRoot,
                temporaryDirectory->path,
                fixture.first,
                &copy
                ),
            std::string("could not copy ") + std::string(fixture.first)
                + " for migration verification"
            );
        if (Filesystem::exists(copy))
        {
            passed &= verifyMigratedFixture(copy, fixture.first, fixture.second);
        }
    }

    struct RejectedFixture
    {
        std::string_view fileName;
        bool sourceMustRemainUnchanged;
        std::optional<int> schemaVersionAfterFailure;
        std::string_view expectedMessage;
    };
    const std::array<RejectedFixture, 4> rejectedFixtures{
        RejectedFixture{
            "migration-invalid.db",
            true,
            std::nullopt,
            "Database migration 3"
        },
        RejectedFixture{
            "migration-rollback.db",
            false,
            3,
            "Database migration 4"
        },
        RejectedFixture{
            "newer-schema.tps",
            true,
            std::nullopt,
            "schema version 7"
        },
        RejectedFixture{
            "corrupt.tps",
            true,
            std::nullopt,
            "file is not a database"
        }
    };
    for (const RejectedFixture& fixture : rejectedFixtures)
    {
        Filesystem::path copy;
        passed &= expect(
            copyFixture(
                fixtureRoot,
                temporaryDirectory->path,
                fixture.fileName,
                &copy
                ),
            std::string("could not copy ") + std::string(fixture.fileName)
                + " for rejection verification"
            );
        if (Filesystem::exists(copy))
        {
            passed &= verifyRejectedFixture(
                copy,
                fixture.fileName,
                fixture.sourceMustRemainUnchanged,
                fixture.schemaVersionAfterFailure,
                fixture.expectedMessage
                );
        }
    }

    return passed ? 0 : 1;
}
