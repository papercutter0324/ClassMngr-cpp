#include "classmngr/engine/class_info_service.h"

#include "classmngr/engine/class_info_validator.h"
#include "classmngr/engine/sqlite_database.h"

#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace classmngr::engine
{
namespace
{
Error error(
    ErrorCode code,
    std::string message
    )
{
    return {code, std::move(message), std::nullopt};
}

Status validClassId(
    int classId,
    std::string_view action
    )
{
    if (classId > 0)
    {
        return {};
    }
    return std::unexpected(error(
        ErrorCode::InvalidArgument,
        std::string(action) + " requires a positive class id."
        ));
}

std::string validationMessage(const ValidationResult& validation)
{
    std::string message = "Class information validation failed";
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

Result<bool> exists(
    SqliteDatabase& database,
    std::string_view table,
    int id,
    std::string_view label
    )
{
    const auto rows = database.query(
        std::string("SELECT EXISTS(SELECT 1 FROM ")
            + std::string(table) + " WHERE id=?)",
        SqliteParameters{SqliteValue{std::int64_t{id}}}
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.size() != 1 || rows->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected " + std::string(label)
                + " existence result."
            ));
    }
    const auto* value = std::get_if<std::int64_t>(
        &rows->rows.front().values.front()
        );
    if (value == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-integer " + std::string(label)
                + " existence result."
            ));
    }
    return *value != 0;
}

Error notFound(
    std::string_view label,
    int id
    )
{
    return error(
        ErrorCode::NotFound,
        "No " + std::string(label) + " exists for id "
            + std::to_string(id) + "."
        );
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
        "SQLite returned a non-text class information "
            + std::string(column) + " value."
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
            "SQLite returned an invalid class information "
                + std::string(column) + " value."
            ));
    }
    return static_cast<int>(*integer);
}

std::string canonicalChoice(
    std::string value,
    std::initializer_list<std::string_view> choices
    )
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
    value = value.substr(first, last - first);

    auto upper = [](char character) {
        return static_cast<char>(
            std::toupper(static_cast<unsigned char>(character))
            );
    };
    for (const std::string_view choice : choices)
    {
        if (value.size() != choice.size())
        {
            continue;
        }
        bool matches = true;
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (upper(value[index]) != upper(choice[index]))
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            return std::string(choice);
        }
    }
    return value;
}

Result<ClassTime> timeFromRow(const SqliteRow& row)
{
    if (row.values.size() != 3)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected class time row shape."
            ));
    }
    const Result<std::string> day = textValue(row.values[0], "day");
    const Result<std::string> start = textValue(row.values[1], "start_time");
    const Result<std::string> end = textValue(row.values[2], "end_time");
    if (!day)
    {
        return std::unexpected(day.error());
    }
    if (!start)
    {
        return std::unexpected(start.error());
    }
    if (!end)
    {
        return std::unexpected(end.error());
    }
    return ClassTime{*day, *start, *end};
}

Status loadTimes(
    SqliteDatabase& database,
    int classId,
    std::string_view table,
    std::vector<ClassTime>& destination
    )
{
    const auto rows = database.query(
        std::string("SELECT day, start_time, end_time FROM ")
            + std::string(table) + " WHERE class_id=? ORDER BY id",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    destination.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<ClassTime> time = timeFromRow(row);
        if (!time)
        {
            return std::unexpected(time.error());
        }
        destination.push_back(*time);
    }
    return {};
}

SqliteValue nullableTeacherId(int teacherId)
{
    return teacherId > 0
        ? SqliteValue{std::int64_t{teacherId}}
        : SqliteValue{std::monostate{}};
}
} // namespace

ClassInfoService::ClassInfoService(SqliteDatabase& database)
    : m_database(database)
{
}

Status ClassInfoService::save(const ClassInfo& info)
{
    const ClassInfo normalized = ClassInfoValidator::normalized(info);
    const ValidationResult validation = ClassInfoValidator::validate(normalized);
    if (validation.hasErrors())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            validationMessage(validation)
            ));
    }

    const Result<bool> classIsPresent = exists(
        m_database,
        "classes",
        normalized.classId,
        "class"
        );
    if (!classIsPresent)
    {
        return std::unexpected(classIsPresent.error());
    }
    if (!*classIsPresent)
    {
        return std::unexpected(notFound("class", normalized.classId));
    }

    if (normalized.teacherId > 0)
    {
        const Result<bool> teacherIsPresent = exists(
            m_database,
            "teachers",
            normalized.teacherId,
            "teacher"
            );
        if (!teacherIsPresent)
        {
            return std::unexpected(teacherIsPresent.error());
        }
        if (!*teacherIsPresent)
        {
            return std::unexpected(notFound("teacher", normalized.teacherId));
        }
    }

    Result<SqliteTransaction> transaction = m_database.beginTransaction();
    if (!transaction)
    {
        return std::unexpected(transaction.error());
    }

    const Status upsert = m_database.execute(
        "INSERT INTO class_info ("
        "class_id, teacher_id, class_grade, class_level, reading_book, "
        "essay_book, class_color, font_color, notes, time_filler_activities"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(class_id) DO UPDATE SET "
        "teacher_id=excluded.teacher_id, class_grade=excluded.class_grade, "
        "class_level=excluded.class_level, reading_book=excluded.reading_book, "
        "essay_book=excluded.essay_book, class_color=excluded.class_color, "
        "font_color=excluded.font_color, notes=excluded.notes, "
        "time_filler_activities=excluded.time_filler_activities",
        SqliteParameters{
            SqliteValue{std::int64_t{normalized.classId}},
            nullableTeacherId(normalized.teacherId),
            SqliteValue{normalized.classGrade},
            SqliteValue{normalized.classLevel},
            SqliteValue{normalized.readingBook},
            SqliteValue{normalized.essayBook},
            SqliteValue{normalized.classColor},
            SqliteValue{normalized.fontColor},
            SqliteValue{normalized.notes},
            SqliteValue{normalized.timeFillerActivities}
        }
        );
    if (!upsert)
    {
        return upsert;
    }

    for (const auto [table, times] : {
             std::pair<std::string_view, const std::vector<ClassTime>&>{
                 "class_times", normalized.classTimes
             },
             std::pair<std::string_view, const std::vector<ClassTime>&>{
                 "class_intensive_times", normalized.intensiveTimes
             }
         })
    {
        const Status deleted = m_database.execute(
            std::string("DELETE FROM ") + std::string(table)
                + " WHERE class_id=?",
            SqliteParameters{SqliteValue{std::int64_t{normalized.classId}}}
            );
        if (!deleted)
        {
            return deleted;
        }

        for (const ClassTime& time : times)
        {
            const Status inserted = m_database.execute(
                std::string("INSERT INTO ") + std::string(table)
                    + " (class_id, day, start_time, end_time) "
                      "VALUES (?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{std::int64_t{normalized.classId}},
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
    }

    return transaction->commit();
}

Status ClassInfoService::saveNotes(
    int classId,
    std::string_view notes,
    std::string_view timeFillerActivities
    )
{
    const ValidationResult validation = ClassInfoValidator::validateNotes(
        classId,
        notes,
        timeFillerActivities
        );
    if (validation.hasErrors())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            validationMessage(validation)
            ));
    }

    const Result<bool> classIsPresent = exists(
        m_database,
        "classes",
        classId,
        "class"
        );
    if (!classIsPresent)
    {
        return std::unexpected(classIsPresent.error());
    }
    if (!*classIsPresent)
    {
        return std::unexpected(notFound("class", classId));
    }

    return m_database.execute(
        "INSERT INTO class_info (class_id, notes, time_filler_activities) "
        "VALUES (?, ?, ?) ON CONFLICT(class_id) DO UPDATE SET "
        "notes=excluded.notes, "
        "time_filler_activities=excluded.time_filler_activities",
        SqliteParameters{
            SqliteValue{std::int64_t{classId}},
            SqliteValue{std::string(notes)},
            SqliteValue{std::string(timeFillerActivities)}
        }
        );
}

Result<ClassInfo> ClassInfoService::load(int classId)
{
    const Status valid = validClassId(classId, "Loading class information");
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const Result<bool> classIsPresent = exists(
        m_database,
        "classes",
        classId,
        "class"
        );
    if (!classIsPresent)
    {
        return std::unexpected(classIsPresent.error());
    }
    if (!*classIsPresent)
    {
        return std::unexpected(notFound("class", classId));
    }

    ClassInfo info;
    info.classId = classId;
    const auto rows = m_database.query(
        "SELECT ci.teacher_id, ci.class_grade, ci.class_level, "
        "ci.reading_book, ci.essay_book, ci.class_color, ci.font_color, "
        "ci.notes, ci.time_filler_activities, t.teacher_kr, t.teacher_en, "
        "t.preferred_name, t.room_number, t.wifi_name, t.wifi_password, "
        "t.internet_type, t.zoom_id, t.zoom_password, t.projection_type "
        "FROM class_info ci LEFT JOIN teachers t ON ci.teacher_id=t.id "
        "WHERE ci.class_id=?",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.size() > 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned multiple class information rows."
            ));
    }

    if (!rows->rows.empty())
    {
        const SqliteRow& row = rows->rows.front();
        if (row.values.size() != 19)
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an unexpected class information row shape."
                ));
        }

        const Result<int> teacherId = integerValue(row.values[0], "teacher_id");
        if (!teacherId)
        {
            return std::unexpected(teacherId.error());
        }
        info.teacherId = *teacherId;

        std::string* const classFields[] = {
            &info.classGrade,
            &info.classLevel,
            &info.readingBook,
            &info.essayBook,
            &info.notes,
            &info.timeFillerActivities
        };
        const std::string_view classColumns[] = {
            "class_grade",
            "class_level",
            "reading_book",
            "essay_book",
            "notes",
            "time_filler_activities"
        };
        for (std::size_t index = 0; index < 4; ++index)
        {
            const Result<std::string> value = textValue(
                row.values[index + 1],
                classColumns[index]
                );
            if (!value)
            {
                return std::unexpected(value.error());
            }
            *classFields[index] = *value;
        }

        const Result<std::string> classColor = textValue(row.values[5], "class_color");
        const Result<std::string> fontColor = textValue(row.values[6], "font_color");
        if (!classColor)
        {
            return std::unexpected(classColor.error());
        }
        if (!fontColor)
        {
            return std::unexpected(fontColor.error());
        }
        if (!classColor->empty())
        {
            info.classColor = *classColor;
        }
        if (!fontColor->empty())
        {
            info.fontColor = *fontColor;
        }

        for (std::size_t index = 4; index < 6; ++index)
        {
            const Result<std::string> value = textValue(
                row.values[index + 3],
                classColumns[index]
                );
            if (!value)
            {
                return std::unexpected(value.error());
            }
            *classFields[index] = *value;
        }

        std::string* const teacherFields[] = {
            &info.teacherKr,
            &info.teacherEn,
            &info.teacherPreferredName,
            &info.roomNumber,
            &info.wifiName,
            &info.wifiPassword,
            &info.internetType,
            &info.zoomId,
            &info.zoomPassword,
            &info.projectionType
        };
        const std::string_view teacherColumns[] = {
            "teacher_kr", "teacher_en", "preferred_name", "room_number",
            "wifi_name", "wifi_password", "internet_type", "zoom_id",
            "zoom_password", "projection_type"
        };
        for (std::size_t index = 0; index < 10; ++index)
        {
            const Result<std::string> value = textValue(
                row.values[index + 9],
                teacherColumns[index]
                );
            if (!value)
            {
                return std::unexpected(value.error());
            }
            *teacherFields[index] = *value;
        }

        info.internetType = canonicalChoice(
            info.internetType,
            {"WiFi", "LAN", "Both", "N/A"}
            );
        info.projectionType = canonicalChoice(
            info.projectionType,
            {"HDMI", "Zoom", "Any", "N/A"}
            );
    }

    const Status regularTimes = loadTimes(
        m_database,
        classId,
        "class_times",
        info.classTimes
        );
    if (!regularTimes)
    {
        return std::unexpected(regularTimes.error());
    }
    const Status intensiveTimes = loadTimes(
        m_database,
        classId,
        "class_intensive_times",
        info.intensiveTimes
        );
    if (!intensiveTimes)
    {
        return std::unexpected(intensiveTimes.error());
    }
    return info;
}

} // namespace classmngr::engine
