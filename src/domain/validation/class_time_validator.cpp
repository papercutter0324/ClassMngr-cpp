#include "class_time_validator.h"

#include "classmngr/engine/class_time_validator.h"

#include <QByteArray>
#include <QHash>
#include <QVariantList>

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

classmngr::engine::ClassTime toEngine(const ClassTime& time)
{
    return {
        .day = toUtf8(time.day),
        .startTime = toUtf8(time.startTime),
        .endTime = toUtf8(time.endTime)
    };
}

ClassTime fromEngine(const classmngr::engine::ClassTime& time)
{
    return {
        .day = fromUtf8(time.day),
        .startTime = fromUtf8(time.startTime),
        .endTime = fromUtf8(time.endTime)
    };
}

QString fieldName(
    const QString& prefix,
    int row,
    const QString& field
    )
{
    return QStringLiteral("%1[%2].%3").arg(prefix).arg(row).arg(field);
}

struct FieldMetadata
{
    int row = -1;
    int column = -1;
};

QHash<QString, FieldMetadata> fieldMetadata(
    const QList<ClassTime>& times,
    const QString& fieldPrefix
    )
{
    QHash<QString, FieldMetadata> result;
    for (int row = 0; row < times.size(); ++row)
    {
        result.insert(
            fieldName(fieldPrefix, row, QStringLiteral("day")),
            {.row = row, .column = 0}
            );
        result.insert(
            fieldName(fieldPrefix, row, QStringLiteral("startTime")),
            {.row = row, .column = 1}
            );
        result.insert(
            fieldName(fieldPrefix, row, QStringLiteral("endTime")),
            {.row = row, .column = 2}
            );
    }
    return result;
}

std::string normalizedSlotKey(
    const classmngr::engine::ClassTime& time
    )
{
    const classmngr::engine::ClassTime normalized =
        classmngr::engine::ClassTimeValidator::normalized(time);
    return normalized.day
        + '|'
        + normalized.startTime
        + '|'
        + normalized.endTime;
}

using DuplicateSlotKeys = std::map<int, std::string>;
using DuplicateRowsBySlot = std::map<std::string, QVariantList>;

void collectDuplicateRows(
    const classmngr::engine::ValidationResult& validation,
    const std::vector<classmngr::engine::ClassTime>& times,
    const QHash<QString, FieldMetadata>& fields,
    DuplicateSlotKeys& slotKeys,
    DuplicateRowsBySlot& rowsBySlot
    )
{
    for (const classmngr::engine::ValidationIssue& source :
         validation.issues())
    {
        if (source.code != "class_time.duplicate_slot")
        {
            continue;
        }

        const auto field = fields.constFind(fromUtf8(source.field));
        if (field == fields.cend())
        {
            continue;
        }

        const int row = field.value().row;
        if (row < 0 || static_cast<std::size_t>(row) >= times.size())
        {
            continue;
        }

        auto slot = slotKeys.find(row);
        if (slot == slotKeys.end())
        {
            slot = slotKeys.emplace(row, normalizedSlotKey(times.at(row))).first;
        }

        QVariantList& duplicateRows = rowsBySlot[slot->second];
        if (!duplicateRows.contains(row))
        {
            duplicateRows.append(row);
        }
    }
}

void restoreArguments(
    ValidationIssue& issue,
    const ClassTime& time,
    const FieldMetadata& field,
    const DuplicateSlotKeys& duplicateSlotKeys,
    const DuplicateRowsBySlot& duplicateRowsBySlot
    )
{
    if (issue.code == QStringLiteral("schedule.weekday.invalid")
        && field.column == 0)
    {
        issue.arguments = {
            {QStringLiteral("value"), time.day}
        };
    }
    else if (issue.code == QStringLiteral("schedule.time.invalid_format"))
    {
        const QString* value = field.column == 1
            ? &time.startTime
            : field.column == 2 ? &time.endTime : nullptr;
        if (value != nullptr)
        {
            issue.arguments = {
                {QStringLiteral("value"), *value}
            };
        }
    }
    else if (issue.code == QStringLiteral("schedule.time.end_not_after_start")
             && field.column == 2)
    {
        issue.arguments = {
            {QStringLiteral("start"), time.startTime},
            {QStringLiteral("end"), time.endTime}
        };
    }
    else if (issue.code == QStringLiteral("class_time.duplicate_slot")
             && field.column == 1)
    {
        const auto slot = duplicateSlotKeys.find(field.row);
        if (slot == duplicateSlotKeys.end())
        {
            return;
        }

        const auto rows = duplicateRowsBySlot.find(slot->second);
        if (rows != duplicateRowsBySlot.end())
        {
            issue.arguments = {
                {QStringLiteral("duplicateRows"), rows->second}
            };
        }
    }
}

ValidationResult fromEngine(
    const classmngr::engine::ValidationResult& validation,
    const QList<ClassTime>& times,
    const QHash<QString, FieldMetadata>& fields,
    const DuplicateSlotKeys& duplicateSlotKeys,
    const DuplicateRowsBySlot& duplicateRowsBySlot
    )
{
    ValidationResult result;
    for (const classmngr::engine::ValidationIssue& source :
         validation.issues())
    {
        ValidationIssue issue{
            .code = fromUtf8(source.code),
            .field = fromUtf8(source.field),
            .row = source.row,
            .column = source.column,
            .severity = source.isWarning()
                ? ValidationSeverity::Warning
                : ValidationSeverity::Error
        };

        const auto field = fields.constFind(issue.field);
        if (field != fields.cend())
        {
            if (issue.row < 0)
            {
                issue.row = field.value().row;
            }
            if (issue.column < 0)
            {
                issue.column = field.value().column;
            }
            restoreArguments(
                issue,
                times.at(field.value().row),
                field.value(),
                duplicateSlotKeys,
                duplicateRowsBySlot
                );
        }

        result.add(std::move(issue));
    }
    return result;
}
} // namespace

ClassTime ClassTimeValidator::normalized(const ClassTime& time)
{
    return fromEngine(
        classmngr::engine::ClassTimeValidator::normalized(toEngine(time))
        );
}

ValidationResult ClassTimeValidator::validate(
    const QList<ClassTime>& times,
    const QString& fieldPrefix
    )
{
    std::vector<classmngr::engine::ClassTime> engineTimes;
    engineTimes.reserve(static_cast<std::size_t>(times.size()));
    for (const ClassTime& time : times)
    {
        engineTimes.push_back(toEngine(time));
    }

    const std::string engineFieldPrefix = toUtf8(fieldPrefix);
    const classmngr::engine::ValidationResult validation =
        classmngr::engine::ClassTimeValidator::validate(
            engineTimes,
            engineFieldPrefix
            );

    const QHash<QString, FieldMetadata> fields = fieldMetadata(
        times,
        fieldPrefix
        );
    DuplicateSlotKeys duplicateSlotKeys;
    DuplicateRowsBySlot duplicateRowsBySlot;
    collectDuplicateRows(
        validation,
        engineTimes,
        fields,
        duplicateSlotKeys,
        duplicateRowsBySlot
        );

    return fromEngine(
        validation,
        times,
        fields,
        duplicateSlotKeys,
        duplicateRowsBySlot
        );
}
