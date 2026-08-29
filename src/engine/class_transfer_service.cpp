#include "classmngr/engine/class_transfer_service.h"

#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_info_validator.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_schedule_service.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/teacher_service.h"
#include "classmngr/engine/teacher_validator.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr std::size_t SpeakingEvaluationRowCountLocal =
    SpeakingEvaluationRowCount;
constexpr std::size_t SpeakingEvaluationColumnCountLocal =
    SpeakingEvaluationColumnCount;

Error error(
    ErrorCode code,
    std::string message
    )
{
    return {code, std::move(message), std::nullopt};
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size()
           && std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
           && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string collapseWhitespace(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;

    for (const char character : value)
    {
        if (std::isspace(static_cast<unsigned char>(character)) != 0)
        {
            pendingSpace = !result.empty();
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(character);
    }

    return result;
}

std::string normalized(std::string_view value)
{
    std::string result = collapseWhitespace(value);
    for (char& character : result)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return result;
}

Result<std::string> textValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    if (const auto* text = std::get_if<std::string>(&value))
    {
        return *text;
    }
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::string{};
    }

    return std::unexpected(error(
        ErrorCode::Schema,
        "SQLite returned a non-text " + std::string(column) + " value."
        ));
}

Result<int> integerValue(
    const SqliteValue& value,
    std::string_view column,
    int nullValue = -1
    )
{
    if (std::holds_alternative<std::monostate>(value))
    {
        return nullValue;
    }

    const auto* integer = std::get_if<std::int64_t>(&value);
    if (integer == nullptr
        || *integer < std::numeric_limits<int>::min()
        || *integer > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid " + std::string(column) + " value."
            ));
    }

    return static_cast<int>(*integer);
}

std::string validationMessage(
    std::string_view subject,
    const ValidationResult& validation
    )
{
    std::string message(subject);
    message += " validation failed";
    bool first = true;
    for (const ValidationIssue& issue : validation.issues())
    {
        if (!issue.isError())
        {
            continue;
        }

        message += first ? ": " : "; ";
        first = false;
        message += issue.code;
        if (!issue.field.empty())
        {
            message += " (";
            message += issue.field;
            message += ')';
        }
    }
    message += '.';
    return message;
}

void clearJoinedTeacherFields(ClassInfo& info)
{
    info.teacherKr.clear();
    info.teacherEn.clear();
    info.teacherPreferredName.clear();
    info.roomNumber.clear();
    info.wifiName.clear();
    info.wifiPassword.clear();
    info.internetType.clear();
    info.zoomId.clear();
    info.zoomPassword.clear();
    info.projectionType.clear();
}

Status validateRoster(
    const Roster& roster,
    std::string_view context
    )
{
    if (roster.columnWidths.size() != roster.columns.size())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            std::string(context) + ".columnWidths must match the column count."
            ));
    }

    for (std::size_t row = 0; row < roster.rows.size(); ++row)
    {
        if (roster.rows[row].size() != roster.columns.size())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                std::string(context) + ".rows[" + std::to_string(row)
                    + "] must match the column count."
                ));
        }
    }

    return {};
}

Status validateEvaluations(
    const std::vector<ClassTransferEvaluation>& evaluations,
    std::string_view context
    )
{
    std::set<std::string> names;
    for (std::size_t index = 0; index < evaluations.size(); ++index)
    {
        const ClassTransferEvaluation& evaluation = evaluations[index];
        if (trimAsciiWhitespace(evaluation.name).empty())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                std::string(context) + "[" + std::to_string(index)
                    + "].name must not be empty."
                ));
        }
        if (!names.insert(evaluation.name).second)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                std::string(context) + " contains a duplicate evaluation name: "
                    + evaluation.name
                ));
        }
        if (evaluation.rows.size() > SpeakingEvaluationRowCountLocal)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                std::string(context) + "[" + std::to_string(index)
                    + "].rows has too many rows."
                ));
        }
        for (std::size_t row = 0; row < evaluation.rows.size(); ++row)
        {
            if (evaluation.rows[row].size()
                    != SpeakingEvaluationColumnCountLocal)
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    std::string(context) + "[" + std::to_string(index)
                        + "].rows[" + std::to_string(row)
                        + "] must contain "
                        + std::to_string(SpeakingEvaluationColumnCountLocal)
                        + " columns."
                    ));
            }
        }
    }

    return {};
}

Result<ClassTransferPackage> normalizedPackage(
    const ClassTransferPackage& source
    )
{
    if (source.version != ClassTransferPackage::CurrentVersion)
    {
        return std::unexpected(error(
            ErrorCode::Unsupported,
            "Unsupported class package version: "
                + std::to_string(source.version) + "."
            ));
    }
    if (source.classes.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "The class package does not contain any classes."
            ));
    }

    ClassTransferPackage result = source;
    std::set<std::string> teacherKeys;
    for (std::size_t index = 0; index < result.teachers.size(); ++index)
    {
        ClassTransferTeacher& transferTeacher = result.teachers[index];
        if (trimAsciiWhitespace(transferTeacher.key).empty())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "teachers[" + std::to_string(index) + "].key must not be empty."
                ));
        }
        if (!teacherKeys.insert(transferTeacher.key).second)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Duplicate teacher key: " + transferTeacher.key
                ));
        }

        transferTeacher.teacher = TeacherValidator::normalized(
            transferTeacher.teacher);
        transferTeacher.teacher.id = -1;
        const ValidationResult validation = TeacherValidator::validate(
            transferTeacher.teacher);
        if (validation.hasErrors())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                validationMessage(
                    "Teacher " + transferTeacher.key,
                    validation
                    )
                ));
        }
    }

    std::set<std::string> classKeys;
    for (std::size_t index = 0; index < result.classes.size(); ++index)
    {
        ClassTransferClass& transferClass = result.classes[index];
        if (trimAsciiWhitespace(transferClass.key).empty())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "classes[" + std::to_string(index) + "].key must not be empty."
                ));
        }
        if (!classKeys.insert(transferClass.key).second)
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Duplicate class key: " + transferClass.key
                ));
        }
        if (!transferClass.teacherKey.empty()
            && !teacherKeys.contains(transferClass.teacherKey))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Class " + transferClass.key
                    + " references an unknown teacher key: "
                    + transferClass.teacherKey
                ));
        }

        const Status roster = validateRoster(
            transferClass.roster,
            "classes[" + std::to_string(index) + "].roster"
            );
        if (!roster)
        {
            return std::unexpected(roster.error());
        }
        const Status evaluations = validateEvaluations(
            transferClass.evaluations,
            "classes[" + std::to_string(index) + "].speakingEvaluations"
            );
        if (!evaluations)
        {
            return std::unexpected(evaluations.error());
        }

        ClassInfo info = ClassInfoValidator::normalized(transferClass.info);
        // Exported package information deliberately has no local identity.
        // Validate the portable fields with a sentinel identity, then restore
        // the export representation before it reaches persistence.
        info.classId = 1;
        info.teacherId = -1;
        clearJoinedTeacherFields(info);
        const ValidationResult validation = ClassInfoValidator::validate(info);
        if (validation.hasErrors())
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                validationMessage(
                    "Class " + transferClass.key,
                    validation
                    )
                ));
        }
        info.classId = -1;
        info.teacherId = -1;
        transferClass.info = std::move(info);
    }

    return result;
}

const ClassTransferTeacher* packageTeacher(
    const ClassTransferPackage& package,
    std::string_view key
    )
{
    for (const ClassTransferTeacher& teacher : package.teachers)
    {
        if (teacher.key == key)
        {
            return &teacher;
        }
    }
    return nullptr;
}

bool teacherMatches(
    const Teacher& source,
    const Teacher& destination
    )
{
    const std::string sourceEnglish = normalized(source.teacherEn);
    const std::string sourceKorean = normalized(source.teacherKr);
    const std::string destinationEnglish = normalized(destination.teacherEn);
    const std::string destinationKorean = normalized(destination.teacherKr);

    if (!sourceEnglish.empty() && !sourceKorean.empty())
    {
        return sourceEnglish == destinationEnglish
            && sourceKorean == destinationKorean;
    }
    if (!sourceEnglish.empty())
    {
        return sourceEnglish == destinationEnglish;
    }
    if (!sourceKorean.empty())
    {
        return sourceKorean == destinationKorean;
    }
    return false;
}

std::string transferClassLabel(const ClassTransferClass& transferClass)
{
    const std::string grade = trimAsciiWhitespace(
        transferClass.info.classGrade);
    const std::string level = trimAsciiWhitespace(
        transferClass.info.classLevel);
    std::string course = grade;
    if (!level.empty())
    {
        if (!course.empty())
        {
            course += ' ';
        }
        course += level;
    }
    if (!course.empty())
    {
        return course;
    }

    const std::string name = trimAsciiWhitespace(transferClass.name);
    return name.empty() ? transferClass.key : name;
}

std::string destinationClassLabel(
    const Classroom& classroom,
    const ClassInfo& info
    )
{
    const std::string grade = trimAsciiWhitespace(info.classGrade);
    const std::string level = trimAsciiWhitespace(info.classLevel);
    std::string course = grade;
    if (!level.empty())
    {
        if (!course.empty())
        {
            course += ' ';
        }
        course += level;
    }
    if (!course.empty())
    {
        return course;
    }

    const std::string name = trimAsciiWhitespace(classroom.name);
    return name.empty() ? "Class " + std::to_string(classroom.id) : name;
}

SpeakingEvaluationRows emptyEvaluationRows()
{
    return SpeakingEvaluationRows(
        SpeakingEvaluationRowCountLocal,
        std::vector<std::string>(
            SpeakingEvaluationColumnCountLocal,
            std::string{}
            )
        );
}

Result<Roster> loadRoster(
    SqliteDatabase& database,
    int classId
    )
{
    const auto columnRows = database.query(
        "SELECT name, width FROM roster_columns "
        "WHERE class_id=? ORDER BY position, id",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!columnRows)
    {
        return std::unexpected(columnRows.error());
    }

    Roster roster;
    roster.columns.reserve(columnRows->rows.size());
    roster.columnWidths.reserve(columnRows->rows.size());
    for (const SqliteRow& row : columnRows->rows)
    {
        if (row.values.size() != 2)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an unexpected roster column row shape."
                ));
        }
        const Result<std::string> name = textValue(row.values[0], "name");
        const Result<int> width = integerValue(row.values[1], "width", 0);
        if (!name)
        {
            return std::unexpected(name.error());
        }
        if (!width)
        {
            return std::unexpected(width.error());
        }
        roster.columns.push_back(*name);
        roster.columnWidths.push_back(*width);
    }

    if (roster.columns.empty())
    {
        return roster;
    }

    const auto dataRows = database.query(
        "SELECT row_index, col_index, value FROM roster_data "
        "WHERE class_id=? ORDER BY row_index, col_index",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!dataRows)
    {
        return std::unexpected(dataRows.error());
    }

    for (const SqliteRow& row : dataRows->rows)
    {
        if (row.values.size() != 3)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an unexpected roster data row shape."
                ));
        }
        const Result<int> rowIndex = integerValue(row.values[0], "row_index");
        const Result<int> columnIndex = integerValue(
            row.values[1], "col_index");
        const Result<std::string> value = textValue(row.values[2], "value");
        if (!rowIndex)
        {
            return std::unexpected(rowIndex.error());
        }
        if (!columnIndex)
        {
            return std::unexpected(columnIndex.error());
        }
        if (!value)
        {
            return std::unexpected(value.error());
        }
        if (*rowIndex < 0 || *columnIndex < 0
            || static_cast<std::size_t>(*columnIndex) >= roster.columns.size())
        {
            continue;
        }

        while (roster.rows.size() <= static_cast<std::size_t>(*rowIndex))
        {
            roster.rows.emplace_back(
                roster.columns.size(),
                std::string{}
                );
        }
        roster.rows[static_cast<std::size_t>(*rowIndex)]
            [static_cast<std::size_t>(*columnIndex)] = *value;
    }

    return roster;
}

Result<std::vector<ClassTransferEvaluation>> loadEvaluations(
    SqliteDatabase& database,
    int classId
    )
{
    const auto evaluationRows = database.query(
        "SELECT id, evaluation_name FROM speaking_evaluations "
        "WHERE class_id=? ORDER BY id",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!evaluationRows)
    {
        return std::unexpected(evaluationRows.error());
    }

    std::vector<ClassTransferEvaluation> evaluations;
    evaluations.reserve(evaluationRows->rows.size());
    for (const SqliteRow& evaluationRow : evaluationRows->rows)
    {
        if (evaluationRow.values.size() != 2)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an unexpected speaking evaluation row shape."
                ));
        }
        const Result<int> evaluationId = integerValue(
            evaluationRow.values[0], "evaluation_id");
        const Result<std::string> name = textValue(
            evaluationRow.values[1], "evaluation_name");
        if (!evaluationId)
        {
            return std::unexpected(evaluationId.error());
        }
        if (!name)
        {
            return std::unexpected(name.error());
        }
        if (*evaluationId <= 0)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an invalid speaking evaluation id."
                ));
        }

        ClassTransferEvaluation evaluation;
        evaluation.name = *name;
        evaluation.rows = emptyEvaluationRows();

        const auto dataRows = database.query(
            "SELECT row_index, col_0, col_1, col_2, col_3, col_4, "
            "col_5, col_6, col_7, col_8, col_9, col_10 "
            "FROM speaking_eval_data WHERE evaluation_id=? "
            "ORDER BY row_index",
            SqliteParameters{SqliteValue{std::int64_t{*evaluationId}}}
            );
        if (!dataRows)
        {
            return std::unexpected(dataRows.error());
        }
        for (const SqliteRow& row : dataRows->rows)
        {
            if (row.values.size() != SpeakingEvaluationColumnCountLocal + 1)
            {
                return std::unexpected(error(
                    ErrorCode::Schema,
                    "SQLite returned an unexpected speaking evaluation data "
                    "row shape."
                    ));
            }
            const Result<int> rowIndex = integerValue(
                row.values[0], "row_index");
            if (!rowIndex)
            {
                return std::unexpected(rowIndex.error());
            }
            if (*rowIndex < 0
                || static_cast<std::size_t>(*rowIndex)
                    >= SpeakingEvaluationRowCountLocal)
            {
                continue;
            }
            for (std::size_t column = 0;
                 column < SpeakingEvaluationColumnCountLocal;
                 ++column)
            {
                const Result<std::string> value = textValue(
                    row.values[column + 1],
                    "speaking_eval_data.col_" + std::to_string(column)
                    );
                if (!value)
                {
                    return std::unexpected(value.error());
                }
                evaluation.rows[static_cast<std::size_t>(*rowIndex)][column]
                    = *value;
            }
        }

        evaluations.push_back(std::move(evaluation));
    }

    return evaluations;
}

Result<ClassImportPreview> buildPreview(
    SqliteDatabase& database,
    const ClassTransferPackage& package
    )
{
    TeacherService teacherService(database);
    ClassRepository classRepository(database);
    ClassInfoService classInfoService(database);

    const Result<std::vector<Teacher>> destinationTeachers =
        teacherService.list();
    if (!destinationTeachers)
    {
        return std::unexpected(destinationTeachers.error());
    }
    const Result<std::vector<Classroom>> destinationClasses =
        classRepository.list();
    if (!destinationClasses)
    {
        return std::unexpected(destinationClasses.error());
    }

    ClassImportPreview preview;
    preview.teachers.reserve(package.teachers.size());
    for (const ClassTransferTeacher& packageTeacherEntry : package.teachers)
    {
        ClassImportTeacherPreview entry;
        entry.teacherKey = packageTeacherEntry.key;
        for (const Teacher& destination : *destinationTeachers)
        {
            if (teacherMatches(packageTeacherEntry.teacher, destination))
            {
                entry.matchingTeacherIds.push_back(destination.id);
            }
        }
        preview.teachers.push_back(std::move(entry));
    }

    preview.classes.reserve(package.classes.size());
    for (std::size_t index = 0; index < package.classes.size(); ++index)
    {
        const ClassTransferClass& source = package.classes[index];
        ClassImportClassPreview entry;
        entry.packageClassIndex = static_cast<int>(index);
        const std::string sourceGrade = normalized(source.info.classGrade);
        const std::string sourceLevel = normalized(source.info.classLevel);
        if (sourceGrade.empty() || sourceLevel.empty())
        {
            preview.classes.push_back(std::move(entry));
            continue;
        }

        const ClassTransferTeacher* sourceTeacher = packageTeacher(
            package,
            source.teacherKey
            );
        for (const Classroom& destination : *destinationClasses)
        {
            const Result<ClassInfo> destinationInfo = classInfoService.load(
                destination.id);
            if (!destinationInfo)
            {
                return std::unexpected(destinationInfo.error());
            }
            if (normalized(destinationInfo->classGrade) != sourceGrade
                || normalized(destinationInfo->classLevel) != sourceLevel)
            {
                continue;
            }

            bool sameTeacher = false;
            if (sourceTeacher == nullptr)
            {
                sameTeacher = destinationInfo->teacherId <= 0;
            }
            else if (destinationInfo->teacherId > 0)
            {
                Teacher destinationTeacher;
                destinationTeacher.teacherKr = destinationInfo->teacherKr;
                destinationTeacher.teacherEn = destinationInfo->teacherEn;
                sameTeacher = teacherMatches(
                    sourceTeacher->teacher,
                    destinationTeacher
                    );
            }
            if (sameTeacher)
            {
                entry.matchingClassIds.push_back(destination.id);
            }
        }
        preview.classes.push_back(std::move(entry));
    }

    return preview;
}

struct ValidatedPlan
{
    std::map<int, ClassImportResolution> classes;
    std::map<std::string, TeacherImportResolution> teachers;
};

Result<ValidatedPlan> validatePlan(
    SqliteDatabase& database,
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    const Result<ClassImportPreview> preview = buildPreview(database, package);
    if (!preview)
    {
        return std::unexpected(preview.error());
    }

    std::map<int, std::vector<int>> classMatches;
    for (const ClassImportClassPreview& entry : preview->classes)
    {
        classMatches.emplace(entry.packageClassIndex, entry.matchingClassIds);
    }
    std::map<std::string, std::vector<int>> teacherMatchesByKey;
    for (const ClassImportTeacherPreview& entry : preview->teachers)
    {
        teacherMatchesByKey.emplace(entry.teacherKey, entry.matchingTeacherIds);
    }

    ValidatedPlan validated;
    std::set<int> replacementTargets;
    std::set<int> teacherReplacementTargets;
    for (const ClassImportResolution& resolution : plan.classes)
    {
        const int index = resolution.packageClassIndex;
        if (index < 0
            || static_cast<std::size_t>(index) >= package.classes.size()
            || validated.classes.contains(index))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The class import plan contains an invalid or duplicate "
                "class entry."
                ));
        }

        const auto matches = classMatches.at(index);
        switch (resolution.action)
        {
        case ClassImportAction::Create:
        case ClassImportAction::Skip:
            if (resolution.targetClassId > 0)
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "Only replacement actions may specify a destination class."
                    ));
            }
            break;
        case ClassImportAction::Replace:
            if (std::find(
                    matches.begin(),
                    matches.end(),
                    resolution.targetClassId
                    ) == matches.end())
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "A replacement class is not one of the inferred matches."
                    ));
            }
            if (!replacementTargets.insert(resolution.targetClassId).second)
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "Two package classes cannot replace the same destination "
                    "class."
                    ));
            }
            break;
        default:
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The class import plan contains an unknown class action."
                ));
        }
        validated.classes.emplace(index, resolution);
    }

    if (validated.classes.size() != package.classes.size())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Every package class must have an import action."
            ));
    }

    for (const TeacherImportResolution& resolution : plan.teachers)
    {
        if (trimAsciiWhitespace(resolution.teacherKey).empty()
            || !teacherMatchesByKey.contains(resolution.teacherKey)
            || validated.teachers.contains(resolution.teacherKey))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The teacher import plan contains an invalid or duplicate "
                "teacher entry."
                ));
        }

        const auto matches = teacherMatchesByKey.at(resolution.teacherKey);
        switch (resolution.action)
        {
        case TeacherImportAction::Create:
            if (matches.size() == 1 || resolution.targetTeacherId > 0)
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "An unambiguous teacher match must reuse the local teacher."
                    ));
            }
            break;
        case TeacherImportAction::KeepExisting:
        case TeacherImportAction::ReplaceExisting:
            if (std::find(
                    matches.begin(),
                    matches.end(),
                    resolution.targetTeacherId
                    ) == matches.end())
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "A selected teacher is not one of the inferred matches."
                    ));
            }
            if (resolution.action == TeacherImportAction::ReplaceExisting
                && !teacherReplacementTargets.insert(
                    resolution.targetTeacherId).second)
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "Two different package teachers cannot replace the same "
                    "local teacher."
                    ));
            }
            break;
        default:
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "The teacher import plan contains an unknown teacher action."
                ));
        }
        validated.teachers.emplace(resolution.teacherKey, resolution);
    }

    if (validated.teachers.size() != package.teachers.size())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Every package teacher must have an import action."
            ));
    }

    return validated;
}

std::string conflictDescription(
    std::string_view scheduleLabel,
    const ClassConflict& conflict
    )
{
    std::string result(scheduleLabel);
    result += ": ";
    result += conflict.className;
    result += " — ";
    result += conflict.day;
    result += ' ';
    result += conflict.startTime;
    result += "–";
    result += conflict.endTime;
    result += " conflicts with ";
    result += conflict.conflictingClassName;
    return result;
}

Status preflightSchedules(
    SqliteDatabase& database,
    const ClassTransferPackage& package,
    const ValidatedPlan& plan
    )
{
    std::set<int> replacedClassIds;
    for (const auto& [index, resolution] : plan.classes)
    {
        (void)index;
        if (resolution.action == ClassImportAction::Replace)
        {
            replacedClassIds.insert(resolution.targetClassId);
        }
    }

    std::vector<ClassScheduleEntry> importedRegular;
    std::vector<ClassScheduleEntry> importedIntensive;
    for (std::size_t index = 0; index < package.classes.size(); ++index)
    {
        const ClassImportResolution resolution = plan.classes.at(
            static_cast<int>(index));
        if (resolution.action == ClassImportAction::Skip)
        {
            continue;
        }

        const ClassTransferClass& transferClass = package.classes[index];
        const std::string label = transferClassLabel(transferClass);
        for (const ClassTime& time : transferClass.info.classTimes)
        {
            importedRegular.push_back({-1, label, time});
        }
        for (const ClassTime& time : transferClass.info.intensiveTimes)
        {
            importedIntensive.push_back({-1, label, time});
        }
    }

    ClassRepository classRepository(database);
    ClassScheduleService scheduleService(database);
    const Result<std::vector<Classroom>> destinationClasses =
        classRepository.list();
    if (!destinationClasses)
    {
        return std::unexpected(destinationClasses.error());
    }
    const Result<std::vector<ClassInfo>> destinationInfos =
        scheduleService.loadScheduleClassInfos();
    if (!destinationInfos)
    {
        return std::unexpected(destinationInfos.error());
    }

    std::map<int, const Classroom*> classesById;
    for (const Classroom& classroom : *destinationClasses)
    {
        classesById.emplace(classroom.id, &classroom);
    }

    std::vector<ClassScheduleEntry> existingRegular;
    std::vector<ClassScheduleEntry> existingIntensive;
    for (const ClassInfo& info : *destinationInfos)
    {
        if (replacedClassIds.contains(info.classId))
        {
            continue;
        }
        const auto classroom = classesById.find(info.classId);
        if (classroom == classesById.end())
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned schedule information for an unknown class."
                ));
        }
        const std::string label = destinationClassLabel(
            *classroom->second,
            info
            );
        for (const ClassTime& time : info.classTimes)
        {
            existingRegular.push_back({info.classId, label, time});
        }
        for (const ClassTime& time : info.intensiveTimes)
        {
            existingIntensive.push_back({info.classId, label, time});
        }
    }

    const auto check = [&scheduleService](
        const std::vector<ClassScheduleEntry>& imported,
        const std::vector<ClassScheduleEntry>& existing,
        std::string_view label
        ) -> Status
    {
        const std::vector<ClassConflict> conflicts =
            scheduleService.findConflicts(imported, existing);
        if (conflicts.empty())
        {
            return {};
        }

        std::string message = "Schedule conflicts prevent this import:\n\n";
        for (std::size_t index = 0; index < conflicts.size(); ++index)
        {
            if (index != 0)
            {
                message += '\n';
            }
            message += conflictDescription(label, conflicts[index]);
        }
        return std::unexpected(error(ErrorCode::InvalidFormat, std::move(message)));
    };

    const Status regular = check(
        importedRegular,
        existingRegular,
        "Regular schedule"
        );
    if (!regular)
    {
        return regular;
    }
    return check(importedIntensive, existingIntensive, "Intensive schedule");
}

Status clearClassData(
    SqliteDatabase& database,
    int classId
    )
{
    Status status = database.execute(
        "DELETE FROM speaking_eval_data WHERE evaluation_id IN ("
        "SELECT id FROM speaking_evaluations WHERE class_id=?)",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!status)
    {
        return status;
    }

    for (const std::string_view table : {
             "speaking_evaluations",
             "roster_columns",
             "roster_data",
             "class_info",
             "class_times",
             "class_intensive_times"
         })
    {
        status = database.execute(
            std::string("DELETE FROM ") + std::string(table)
                + " WHERE class_id=?",
            SqliteParameters{SqliteValue{std::int64_t{classId}}}
            );
        if (!status)
        {
            return status;
        }
    }
    return {};
}

Result<int> lastInsertId(
    SqliteDatabase& database,
    std::string_view entity
    )
{
    const auto row = database.query("SELECT last_insert_rowid()");
    if (!row)
    {
        return std::unexpected(row.error());
    }
    if (row->rows.size() != 1 || row->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite did not return the new " + std::string(entity) + " id."
            ));
    }
    const Result<int> id = integerValue(
        row->rows.front().values.front(),
        std::string(entity) + "_id"
        );
    if (!id)
    {
        return std::unexpected(id.error());
    }
    if (*id <= 0)
    {
        return std::unexpected(error(
            ErrorCode::Database,
            "The imported " + std::string(entity)
                + " did not receive a valid id."
            ));
    }
    return id;
}

Status writeClassData(
    SqliteDatabase& database,
    int classId,
    int teacherId,
    const ClassTransferClass& transferClass
    )
{
    const ClassInfo& info = transferClass.info;
    Status status = database.execute(
        "INSERT INTO class_info ("
        "class_id, teacher_id, class_grade, class_level, reading_book, "
        "essay_book, class_color, font_color, notes, time_filler_activities"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        SqliteParameters{
            SqliteValue{std::int64_t{classId}},
            teacherId > 0
                ? SqliteValue{std::int64_t{teacherId}}
                : SqliteValue{std::monostate{}},
            SqliteValue{info.classGrade},
            SqliteValue{info.classLevel},
            SqliteValue{info.readingBook},
            SqliteValue{info.essayBook},
            SqliteValue{info.classColor},
            SqliteValue{info.fontColor},
            SqliteValue{info.notes},
            SqliteValue{info.timeFillerActivities}
        }
        );
    if (!status)
    {
        return status;
    }

    const auto writeTimes = [&database, classId](
        std::string_view table,
        const std::vector<ClassTime>& times
        ) -> Status
    {
        for (const ClassTime& time : times)
        {
            const Status inserted = database.execute(
                std::string("INSERT INTO ") + std::string(table)
                    + " (class_id, day, start_time, end_time) "
                      "VALUES (?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{std::int64_t{classId}},
                    SqliteValue{time.day},
                    SqliteValue{time.startTime},
                    SqliteValue{time.endTime}
                }
                );
            if (!inserted)
            {
                return inserted;
            }
        }
        return {};
    };

    status = writeTimes("class_times", info.classTimes);
    if (!status)
    {
        return status;
    }
    status = writeTimes("class_intensive_times", info.intensiveTimes);
    if (!status)
    {
        return status;
    }

    for (std::size_t column = 0;
         column < transferClass.roster.columns.size();
         ++column)
    {
        status = database.execute(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{classId}},
                SqliteValue{transferClass.roster.columns[column]},
                SqliteValue{std::int64_t{static_cast<std::int64_t>(column)}},
                SqliteValue{std::int64_t{
                    transferClass.roster.columnWidths[column]}}
            }
            );
        if (!status)
        {
            return status;
        }
    }

    for (std::size_t row = 0; row < transferClass.roster.rows.size(); ++row)
    {
        for (std::size_t column = 0;
             column < transferClass.roster.columns.size();
             ++column)
        {
            const std::string& value = transferClass.roster.rows[row][column];
            if (value.empty())
            {
                continue;
            }
            status = database.execute(
                "INSERT INTO roster_data "
                "(class_id, row_index, col_index, value) VALUES (?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{std::int64_t{classId}},
                    SqliteValue{std::int64_t{static_cast<std::int64_t>(row)}},
                    SqliteValue{std::int64_t{static_cast<std::int64_t>(column)}},
                    SqliteValue{value}
                }
                );
            if (!status)
            {
                return status;
            }
        }
    }

    for (const ClassTransferEvaluation& evaluation : transferClass.evaluations)
    {
        status = database.execute(
            "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
            "VALUES (?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{classId}},
                SqliteValue{evaluation.name}
            }
            );
        if (!status)
        {
            return status;
        }
        const Result<int> evaluationId = lastInsertId(
            database,
            "speaking evaluation"
            );
        if (!evaluationId)
        {
            return std::unexpected(evaluationId.error());
        }

        for (std::size_t row = 0;
             row < SpeakingEvaluationRowCountLocal;
             ++row)
        {
            SqliteParameters parameters{
                SqliteValue{std::int64_t{*evaluationId}},
                SqliteValue{std::int64_t{static_cast<std::int64_t>(row)}}
            };
            for (std::size_t column = 0;
                 column < SpeakingEvaluationColumnCountLocal;
                 ++column)
            {
                const std::string value = row < evaluation.rows.size()
                    && column < evaluation.rows[row].size()
                    ? evaluation.rows[row][column]
                    : std::string{};
                parameters.push_back(SqliteValue{value});
            }
            status = database.execute(
                "INSERT INTO speaking_eval_data ("
                "evaluation_id, row_index, col_0, col_1, col_2, col_3, "
                "col_4, col_5, col_6, col_7, col_8, col_9, col_10"
                ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                parameters
                );
            if (!status)
            {
                return status;
            }
        }
    }

    return {};
}
} // namespace

ClassTransferService::ClassTransferService(SqliteDatabase& database)
    : m_database(database)
{
}

Result<ClassTransferPackage> ClassTransferService::buildPackage(
    const std::vector<int>& classIds
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(error(
            ErrorCode::Database,
            "SQLite database is not open."
            ));
    }
    if (classIds.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "No classes were selected."
            ));
    }

    ClassRepository classRepository(m_database);
    ClassInfoService classInfoService(m_database);
    TeacherService teacherService(m_database);
    ClassTransferPackage package;
    package.exportedAtUtc = std::chrono::system_clock::now();
    std::set<int> seenClasses;
    std::map<int, std::string> teacherKeys;

    for (std::size_t index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];
        if (classId <= 0 || !seenClasses.insert(classId).second)
        {
            return std::unexpected(error(
                ErrorCode::InvalidArgument,
                "The class selection contains an invalid or duplicate class."
                ));
        }

        const Result<Classroom> classroom = classRepository.get(classId);
        if (!classroom)
        {
            return std::unexpected(classroom.error());
        }
        const Result<ClassInfo> info = classInfoService.load(classId);
        if (!info)
        {
            return std::unexpected(info.error());
        }
        const Result<Roster> roster = loadRoster(m_database, classId);
        if (!roster)
        {
            return std::unexpected(roster.error());
        }
        const Result<std::vector<ClassTransferEvaluation>> evaluations =
            loadEvaluations(m_database, classId);
        if (!evaluations)
        {
            return std::unexpected(evaluations.error());
        }

        ClassTransferClass transferClass;
        transferClass.key = "class-" + std::to_string(index + 1);
        transferClass.name = classroom->name;
        transferClass.info = *info;
        transferClass.roster = *roster;
        transferClass.evaluations = *evaluations;

        if (info->teacherId > 0)
        {
            const auto key = teacherKeys.find(info->teacherId);
            if (key == teacherKeys.end())
            {
                const Result<Teacher> teacher = teacherService.get(
                    info->teacherId);
                if (!teacher)
                {
                    return std::unexpected(teacher.error());
                }
                const std::string teacherKey = "teacher-"
                    + std::to_string(teacherKeys.size() + 1);
                teacherKeys.emplace(info->teacherId, teacherKey);
                package.teachers.push_back({teacherKey, *teacher});
                transferClass.teacherKey = teacherKey;
            }
            else
            {
                transferClass.teacherKey = key->second;
            }
        }

        transferClass.info.classId = -1;
        transferClass.info.teacherId = -1;
        clearJoinedTeacherFields(transferClass.info);
        package.classes.push_back(std::move(transferClass));
    }

    return package;
}

Result<ClassImportPreview> ClassTransferService::previewImport(
    const ClassTransferPackage& package
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(error(
            ErrorCode::Database,
            "SQLite database is not open."
            ));
    }
    const Result<ClassTransferPackage> normalized = normalizedPackage(package);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }
    return buildPreview(m_database, *normalized);
}

Result<ClassImportSummary> ClassTransferService::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(error(
            ErrorCode::Database,
            "SQLite database is not open."
            ));
    }

    const Result<ClassTransferPackage> normalized = normalizedPackage(package);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }
    const Result<ValidatedPlan> validated = validatePlan(
        m_database,
        *normalized,
        plan
        );
    if (!validated)
    {
        return std::unexpected(validated.error());
    }
    const Status schedulesReady = preflightSchedules(
        m_database,
        *normalized,
        *validated
        );
    if (!schedulesReady)
    {
        return std::unexpected(schedulesReady.error());
    }

    std::set<std::string> usedTeacherKeys;
    for (std::size_t index = 0; index < normalized->classes.size(); ++index)
    {
        const ClassImportResolution resolution = validated->classes.at(
            static_cast<int>(index));
        if (resolution.action != ClassImportAction::Skip
            && !normalized->classes[index].teacherKey.empty())
        {
            usedTeacherKeys.insert(normalized->classes[index].teacherKey);
        }
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(transactionResult.error());
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    TeacherService teacherService(m_database);
    std::map<std::string, int> teacherIds;
    for (const ClassTransferTeacher& transferTeacher : normalized->teachers)
    {
        if (!usedTeacherKeys.contains(transferTeacher.key))
        {
            continue;
        }
        const TeacherImportResolution resolution = validated->teachers.at(
            transferTeacher.key);
        if (resolution.action == TeacherImportAction::Create)
        {
            const Result<int> teacherId = teacherService.create(
                transferTeacher.teacher);
            if (!teacherId)
            {
                return std::unexpected(teacherId.error());
            }
            teacherIds.emplace(transferTeacher.key, *teacherId);
            continue;
        }

        if (resolution.action == TeacherImportAction::ReplaceExisting)
        {
            Teacher replacement = transferTeacher.teacher;
            replacement.id = resolution.targetTeacherId;
            const Status updated = teacherService.update(replacement);
            if (!updated)
            {
                return std::unexpected(updated.error());
            }
        }
        teacherIds.emplace(transferTeacher.key, resolution.targetTeacherId);
    }

    ClassRepository classRepository(m_database);
    ClassImportSummary summary;
    for (std::size_t index = 0; index < normalized->classes.size(); ++index)
    {
        const ClassImportResolution resolution = validated->classes.at(
            static_cast<int>(index));
        const ClassTransferClass& transferClass = normalized->classes[index];
        if (resolution.action == ClassImportAction::Skip)
        {
            ++summary.skippedClassCount;
            continue;
        }

        int classId = resolution.targetClassId;
        if (resolution.action == ClassImportAction::Create)
        {
            const Result<int> created = classRepository.create(
                transferClass.name);
            if (!created)
            {
                return std::unexpected(created.error());
            }
            classId = *created;
            summary.createdClassIds.push_back(classId);
        }
        else
        {
            const Status renamed = classRepository.rename(
                classId,
                transferClass.name
                );
            if (!renamed)
            {
                return std::unexpected(renamed.error());
            }
            const Status cleared = clearClassData(m_database, classId);
            if (!cleared)
            {
                return std::unexpected(cleared.error());
            }
            summary.replacedClassIds.push_back(classId);
        }

        int teacherId = -1;
        if (!transferClass.teacherKey.empty())
        {
            const auto teacher = teacherIds.find(transferClass.teacherKey);
            if (teacher == teacherIds.end())
            {
                return std::unexpected(error(
                    ErrorCode::InvalidFormat,
                    "The imported class references a teacher that was not "
                    "resolved."
                    ));
            }
            teacherId = teacher->second;
        }
        const Status written = writeClassData(
            m_database,
            classId,
            teacherId,
            transferClass
            );
        if (!written)
        {
            return std::unexpected(written.error());
        }
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(committed.error());
    }
    return summary;
}

} // namespace classmngr::engine
