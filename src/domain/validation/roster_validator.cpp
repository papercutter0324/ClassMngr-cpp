#include "roster_validator.h"

#include "classmngr/engine/roster_validator.h"

#include <QByteArray>

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

classmngr::engine::Roster toEngine(const Roster& roster)
{
    classmngr::engine::Roster result;
    result.columns.reserve(static_cast<std::size_t>(roster.columns.size()));
    for (const QString& column : roster.columns)
    {
        result.columns.push_back(toUtf8(column));
    }

    result.columnWidths.reserve(
        static_cast<std::size_t>(roster.columnWidths.size()));
    for (const int width : roster.columnWidths)
    {
        result.columnWidths.push_back(width);
    }

    result.rows.reserve(static_cast<std::size_t>(roster.rows.size()));
    for (const QStringList& sourceRow : roster.rows)
    {
        std::vector<std::string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& value : sourceRow)
        {
            row.push_back(toUtf8(value));
        }
        result.rows.push_back(std::move(row));
    }

    return result;
}

Roster fromEngine(const classmngr::engine::Roster& roster)
{
    Roster result;
    for (const std::string& column : roster.columns)
    {
        result.columns.append(fromUtf8(column));
    }

    for (const int width : roster.columnWidths)
    {
        result.columnWidths.append(width);
    }

    for (const std::vector<std::string>& sourceRow : roster.rows)
    {
        QStringList row;
        for (const std::string& value : sourceRow)
        {
            row.append(fromUtf8(value));
        }
        result.rows.append(row);
    }

    return result;
}

ValidationResult fromEngine(
    const classmngr::engine::ValidationResult& validation
    )
{
    ValidationResult result;
    for (const classmngr::engine::ValidationIssue& source :
         validation.issues())
    {
        result.add({
            .code = fromUtf8(source.code),
            .field = fromUtf8(source.field),
            .row = source.row,
            .column = source.column,
            .severity = source.isWarning()
                ? ValidationSeverity::Warning
                : ValidationSeverity::Error
        });
    }

    return result;
}
} // namespace

Roster RosterValidator::normalized(const Roster& roster)
{
    return fromEngine(
        classmngr::engine::RosterValidator::normalized(toEngine(roster))
        );
}

ValidationResult RosterValidator::validate(
    const Roster& roster,
    bool allowQuestionableKoreanNameLengths
    )
{
    return fromEngine(
        classmngr::engine::RosterValidator::validate(
            toEngine(roster),
            allowQuestionableKoreanNameLengths
            )
        );
}
