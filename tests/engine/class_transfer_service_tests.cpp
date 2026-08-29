#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_transfer_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/teacher_service.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using namespace classmngr::engine;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineClassTransferServiceTests: "
              << message
              << '\n';
    return false;
}

Teacher teacher(std::string name)
{
    Teacher result;
    result.teacherEn = std::move(name);
    result.internetType = "WiFi";
    result.projectionType = "HDMI";
    return result;
}

ClassInfo classInfo(
    int classId,
    int teacherId,
    ClassTime time
    )
{
    ClassInfo result;
    result.classId = classId;
    result.teacherId = teacherId;
    result.classGrade = "E4";
    result.classLevel = "Theseus";
    result.readingBook = "Reading Explorer 2";
    result.essayBook = "4B";
    result.classColor = "#ABCDEF";
    result.fontColor = "#123456";
    result.classTimes = {std::move(time)};
    return result;
}

int countRows(
    SqliteDatabase& database,
    std::string_view sql
    )
{
    const auto result = database.query(sql);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return -1;
    }

    const auto* value = std::get_if<std::int64_t>(
        &result->rows.front().values.front());
    return value == nullptr ? -1 : static_cast<int>(*value);
}

bool hasText(
    SqliteDatabase& database,
    std::string_view sql,
    std::string_view expected
    )
{
    const auto result = database.query(sql);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return false;
    }
    const auto* value = std::get_if<std::string>(
        &result->rows.front().values.front());
    return value != nullptr && *value == expected;
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineClassTransferServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    ClassRepository classes(database);
    ClassInfoService classInfoService(database);
    TeacherService teachers(database);
    ClassTransferService transfers(database);
    bool passed = true;

    const auto aliceId = teachers.create(teacher("Alice Smith"));
    const auto sourceId = classes.create("Source class");
    passed &= expect(
        aliceId && sourceId,
        "source teacher and class could not be created"
        );
    if (!aliceId || !sourceId)
    {
        return 1;
    }

    passed &= expect(
        classInfoService.save(classInfo(
            *sourceId,
            *aliceId,
            ClassTime{"Monday", "4:00 PM", "4:50 PM"}
            )).has_value(),
        "source class information could not be saved"
        );
    passed &= expect(
        database.execute(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{*sourceId}},
                SqliteValue{std::string("Student")},
                SqliteValue{std::int64_t{0}},
                SqliteValue{std::int64_t{180}}
            }
            ).has_value()
            && database.execute(
                "INSERT INTO roster_data "
                "(class_id, row_index, col_index, value) VALUES (?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{std::int64_t{*sourceId}},
                    SqliteValue{std::int64_t{0}},
                    SqliteValue{std::int64_t{0}},
                    SqliteValue{std::string("Student One")}
                }
                ).has_value(),
        "source roster data could not be written"
        );
    passed &= expect(
        database.execute(
            "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
            "VALUES (?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{*sourceId}},
                SqliteValue{std::string("Midterm")}
            }
            ).has_value(),
        "source speaking evaluation could not be created"
        );
    const auto evaluationId = database.query(
        "SELECT last_insert_rowid()"
        );
    passed &= expect(
        evaluationId && evaluationId->rows.size() == 1,
        "source speaking evaluation id could not be read"
        );
    if (!evaluationId || evaluationId->rows.size() != 1)
    {
        return 1;
    }
    const auto* evaluationIdValue = std::get_if<std::int64_t>(
        &evaluationId->rows.front().values.front());
    passed &= expect(
        evaluationIdValue != nullptr
            && database.execute(
                "INSERT INTO speaking_eval_data ("
                "evaluation_id, row_index, col_0, col_1"
                ") VALUES (?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{*evaluationIdValue},
                    SqliteValue{std::int64_t{0}},
                    SqliteValue{std::string("1")},
                    SqliteValue{std::string("Student One")}
                }
                ).has_value(),
        "source speaking evaluation row could not be written"
        );

    const auto packageResult = transfers.buildPackage({*sourceId});
    passed &= expect(
        packageResult
            && packageResult->version == ClassTransferPackage::CurrentVersion
            && packageResult->classes.size() == 1
            && packageResult->teachers.size() == 1
            && packageResult->classes.front().info.classId == -1
            && packageResult->classes.front().info.teacherId == -1
            && packageResult->classes.front().info.teacherEn.empty()
            && packageResult->classes.front().roster.columns
                == std::vector<std::string>{"Student"}
            && packageResult->classes.front().roster.rows.size() == 1
            && packageResult->classes.front().roster.rows.front().front()
                == "Student One"
            && packageResult->classes.front().evaluations.size() == 1
            && packageResult->classes.front().evaluations.front().rows.size()
                == SpeakingEvaluationRowCount
            && packageResult->classes.front().evaluations.front().rows[0][0]
                == "1",
        "class package export did not preserve data or clear identity"
        );
    if (!packageResult)
    {
        return 1;
    }

    ClassTransferPackage package = *packageResult;
    ClassTransferClass createdClass = package.classes.front();
    createdClass.key = "class-2";
    createdClass.name = "Created class";
    createdClass.teacherKey = "teacher-2";
    createdClass.info.classTimes = {
        ClassTime{"Monday", "6:00 PM", "6:50 PM"}
    };
    package.classes.push_back(createdClass);
    package.teachers.push_back({"teacher-2", teacher("Bob Brown")});

    const auto preview = transfers.previewImport(package);
    passed &= expect(
        preview
            && preview->teachers.size() == 2
            && preview->teachers[0].matchingTeacherIds.size() == 1
            && preview->teachers[0].matchingTeacherIds.front() == *aliceId
            && preview->classes.size() == 2
            && preview->classes[0].matchingClassIds.size() == 1
            && preview->classes[0].matchingClassIds.front() == *sourceId
            && preview->classes[1].matchingClassIds.empty(),
        "class import preview did not infer teacher and class matches"
        );

    ClassImportPlan plan;
    plan.classes = {
        {0, ClassImportAction::Replace, *sourceId},
        {1, ClassImportAction::Create, -1}
    };
    plan.teachers = {
        {"teacher-1", TeacherImportAction::KeepExisting, *aliceId},
        {"teacher-2", TeacherImportAction::Create, -1}
    };

    package.classes[0].name = "Replaced class";
    const auto imported = transfers.importClasses(package, plan);
    passed &= expect(
        imported
            && imported->createdClassIds.size() == 1
            && imported->replacedClassIds
                == std::vector<int>{*sourceId}
            && imported->skippedClassCount == 0,
        "class import did not create and replace the planned classes"
        );
    if (!imported || imported->createdClassIds.size() != 1)
    {
        return 1;
    }

    const int createdId = imported->createdClassIds.front();
    const auto replaced = classes.get(*sourceId);
    const auto createdInfo = classInfoService.load(createdId);
    const auto bob = teachers.list();
    passed &= expect(
        replaced && replaced->name == "Replaced class"
            && createdInfo
            && createdInfo->teacherId != *aliceId
            && createdInfo->classTimes.size() == 1
            && createdInfo->classTimes.front().startTime == "6:00 PM"
            && bob && bob->size() == 2
            && countRows(database,
                "SELECT COUNT(*) FROM roster_data WHERE class_id="
                    + std::to_string(*sourceId)) == 1
            && hasText(
                database,
                "SELECT value FROM roster_data WHERE class_id="
                    + std::to_string(*sourceId),
                "Student One"
                )
            && countRows(database,
                "SELECT COUNT(*) FROM speaking_evaluations WHERE class_id="
                    + std::to_string(*sourceId)) == 1
            && countRows(database,
                "SELECT COUNT(*) FROM speaking_eval_data WHERE evaluation_id IN "
                "(SELECT id FROM speaking_evaluations WHERE class_id="
                    + std::to_string(*sourceId) + ")")
                == static_cast<int>(SpeakingEvaluationRowCount),
        "class import did not preserve child tables or assigned teacher"
        );

    ClassTransferPackage skipPackage;
    skipPackage.classes.push_back({
        "class-skip",
        "Skipped class",
        "teacher-skip",
        classInfo(-1, -1, {"Tuesday", "4:00 PM", "4:50 PM"}),
        {},
        {}
    });
    skipPackage.teachers.push_back({"teacher-skip", teacher("Charlie Jones")});
    ClassImportPlan skipPlan;
    skipPlan.classes = {{0, ClassImportAction::Skip, -1}};
    skipPlan.teachers = {
        {"teacher-skip", TeacherImportAction::Create, -1}
    };
    const int classesBeforeSkip = countRows(database, "SELECT COUNT(*) FROM classes");
    const int teachersBeforeSkip = countRows(database, "SELECT COUNT(*) FROM teachers");
    const auto skipped = transfers.importClasses(skipPackage, skipPlan);
    passed &= expect(
        skipped && skipped->skippedClassCount == 1
            && skipped->createdClassIds.empty()
            && countRows(database, "SELECT COUNT(*) FROM classes")
                == classesBeforeSkip
            && countRows(database, "SELECT COUNT(*) FROM teachers")
                == teachersBeforeSkip,
        "skipped class import changed unrelated database state"
        );

    const auto conflictClassId = classes.create("Conflict class");
    passed &= expect(
        conflictClassId
            && classInfoService.save(classInfo(
                *conflictClassId,
                *aliceId,
                ClassTime{"Wednesday", "4:00 PM", "4:50 PM"}
                )).has_value(),
        "conflict fixture could not be created"
        );
    if (!conflictClassId)
    {
        return 1;
    }

    ClassTransferPackage conflictPackage;
    conflictPackage.classes.push_back({
        "class-conflict",
        "Conflicting import",
        "teacher-existing",
        classInfo(-1, -1, {"Wednesday", "4:30 PM", "5:00 PM"}),
        {},
        {}
    });
    conflictPackage.teachers.push_back({
        "teacher-existing",
        teacher("Alice Smith")
    });
    ClassImportPlan conflictPlan;
    conflictPlan.classes = {{0, ClassImportAction::Create, -1}};
    conflictPlan.teachers = {
        {"teacher-existing", TeacherImportAction::KeepExisting, *aliceId}
    };
    const int classesBeforeConflict = countRows(
        database,
        "SELECT COUNT(*) FROM classes"
        );
    const auto conflict = transfers.importClasses(
        conflictPackage,
        conflictPlan
        );
    passed &= expect(
        !conflict
            && conflict.error().code == ErrorCode::InvalidFormat
            && countRows(database, "SELECT COUNT(*) FROM classes")
                == classesBeforeConflict,
        "schedule-conflicting import was not rejected atomically"
        );

    return passed ? 0 : 1;
}
