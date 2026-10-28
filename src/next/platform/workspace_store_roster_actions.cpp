#include "next/platform/workspace_store_roster_actions.h"

#include "data/database/workspace_store.h"
#include "data/repositories/roster_repository.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/application/roster_custom_column_name_policy.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringView>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] std::optional<int> legacyClassId(const Domain::ClassId& id)
{
    int parsed = 0;
    const std::string& value = id.value();
    const auto [end, error] = std::from_chars(
        value.data(), value.data() + value.size(), parsed, 10);
    if (error != std::errc{} || end != value.data() + value.size()
        || parsed <= 0 || std::to_string(parsed) != value)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] QString legacyText(const std::u16string& value)
{
    QString result;
    result.reserve(static_cast<qsizetype>(value.size()));
    for (const char16_t unit : value)
    {
        result.append(QChar(unit));
    }
    return result;
}

[[nodiscard]] std::string legacyText(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] Domain::OperationError repositoryError(
    const RepositoryError& source,
    const char* fallback
    )
{
    const QString diagnostic = toLegacyDiagnostic(source);
    const std::string message = legacyText(diagnostic);
    return {
        .code = repositoryDomainErrorCode(source.code),
        .message = message.empty() ? std::string(fallback) : message,
        .recoverable = source.recoverableOverride.value_or(
            source.code != RepositoryErrorCode::Technical)
    };
}

[[nodiscard]] Domain::OperationError genericError(
    const Domain::ErrorCode code,
    const char* message,
    const bool recoverable = true
    )
{
    return {.code = code, .message = message, .recoverable = recoverable};
}

[[nodiscard]] Application::RosterSnapshot applicationSnapshot(
    const Roster& source
    )
{
    Application::RosterSnapshot result;
    result.columns.reserve(static_cast<std::size_t>(source.columns.size()));
    for (const QString& column : source.columns)
    {
        result.columns.push_back(column.toStdU16String());
    }
    result.columnWidths.reserve(static_cast<std::size_t>(source.columnWidths.size()));
    for (const int width : source.columnWidths)
    {
        result.columnWidths.push_back(
            Application::RosterSnapshotColumnWidth{width});
    }
    result.rows.reserve(static_cast<std::size_t>(source.rows.size()));
    for (const QStringList& sourceRow : source.rows)
    {
        std::vector<std::u16string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& cell : sourceRow)
        {
            row.push_back(cell.toStdU16String());
        }
        result.rows.push_back(std::move(row));
    }
    return result;
}

[[nodiscard]] Roster legacyRoster(const Application::RosterSnapshot& source)
{
    Roster result;
    result.columns.reserve(static_cast<qsizetype>(source.columns.size()));
    for (const std::u16string& column : source.columns)
    {
        result.columns.append(legacyText(column));
    }
    result.columnWidths.reserve(static_cast<qsizetype>(source.columnWidths.size()));
    for (const Application::RosterSnapshotColumnWidth width : source.columnWidths)
    {
        result.columnWidths.append(width.rawValue());
    }
    result.rows.reserve(static_cast<qsizetype>(source.rows.size()));
    for (const auto& sourceRow : source.rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::u16string& cell : sourceRow)
        {
            row.append(legacyText(cell));
        }
        result.rows.append(std::move(row));
    }
    return result;
}

[[nodiscard]] bool caseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QStringView(left.data(), static_cast<qsizetype>(left.size())).compare(
        QStringView(right.data(), static_cast<qsizetype>(right.size())),
        Qt::CaseInsensitive) == 0;
}

class WorkspaceStoreRosterWindowReadPort final
    : public Application::RosterWindowReadPort
{
public:
    explicit WorkspaceStoreRosterWindowReadPort(RosterRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::RosterWindowReadResult readRosterWindow(
        const Application::RosterWindowReadQuery& query
        ) const override
    {
        const auto classId = legacyClassId(query.classId);
        if (!classId)
        {
            return Application::RosterWindowReadResult::failure(genericError(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."));
        }
        if (!m_repository)
        {
            return Application::RosterWindowReadResult::failure(genericError(
                Domain::ErrorCode::Technical,
                "The roster repository is unavailable."));
        }

        try
        {
            const auto loaded = m_repository->loadRosterWindowDetailed(
                *classId,
                static_cast<int>(Application::RosterModeledRowCount));
            if (!loaded)
            {
                return Application::RosterWindowReadResult::failure(
                    repositoryError(loaded.error(), "The roster could not be loaded."));
            }
            if (loaded->parentMissing)
            {
                return Application::RosterWindowReadResult::failure(genericError(
                    Domain::ErrorCode::NotFound,
                    "The selected class was not found."));
            }
            return Application::RosterWindowReadResult::success({
                .roster = applicationSnapshot(loaded->roster),
                .hasRowsOutsideWindow = loaded->hasRowsOutsideWindow
            });
        }
        catch (const std::exception&)
        {
            return Application::RosterWindowReadResult::failure(genericError(
                Domain::ErrorCode::Technical, "The roster could not be loaded."));
        }
        catch (...)
        {
            return Application::RosterWindowReadResult::failure(genericError(
                Domain::ErrorCode::Technical, "The roster could not be loaded."));
        }
    }

private:
    RosterRepository* m_repository = nullptr;
};

class WorkspaceStoreRosterSavePort final : public Application::RosterSavePort
{
public:
    explicit WorkspaceStoreRosterSavePort(RosterRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Domain::Result<void> saveRoster(
        const Application::PreparedRosterSaveRequest& request
        ) const override
    {
        const auto classId = legacyClassId(request.classId);
        if (!classId)
        {
            return Domain::Result<void>::failure(genericError(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."));
        }
        if (!m_repository)
        {
            return Domain::Result<void>::failure(genericError(
                Domain::ErrorCode::Technical,
                "The roster repository is unavailable."));
        }

        try
        {
            QStringList removedColumns;
            for (const std::u16string& column : request.removedCustomColumns)
            {
                removedColumns.append(legacyText(column));
            }
            const RepositoryStatus saved =
                m_repository->saveRosterWindowsWithParentCheck({{
                    .classId = *classId,
                    .roster = legacyRoster(request.roster),
                    .removedCustomColumns = std::move(removedColumns)
                }});
            if (!saved)
            {
                return Domain::Result<void>::failure(repositoryError(
                    saved.error(), "The roster could not be saved."));
            }
            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(genericError(
                Domain::ErrorCode::Technical, "The roster could not be saved."));
        }
        catch (...)
        {
            return Domain::Result<void>::failure(genericError(
                Domain::ErrorCode::Technical, "The roster could not be saved."));
        }
    }

private:
    RosterRepository* m_repository = nullptr;
};

template <typename Result>
[[nodiscard]] Result storeOpenFailure(const RepositoryStatus& opened)
{
    return Result::failure(repositoryError(
        opened.error(), "The selected workspace could not be opened."));
}

} // namespace

Application::RosterWindowReadResult readRosterWindowAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    )
{
    const auto legacyId = legacyClassId(classId);
    if (!legacyId)
    {
        return Application::RosterWindowReadResult::failure(genericError(
            Domain::ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<Application::RosterWindowReadResult>(opened);
    }
    WorkspaceStoreRosterWindowReadPort port(store.rosterRepository());
    return Application::RosterWindowReadUseCase::execute(
        {.classId = classId}, port);
}

Domain::Result<void> saveRosterAtPath(
    const QString& databasePath,
    const Application::RosterSaveRequest& request
    )
{
    const auto legacyId = legacyClassId(request.classId);
    if (!legacyId)
    {
        return Domain::Result<void>::failure(genericError(
            Domain::ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<Domain::Result<void>>(opened);
    }
    WorkspaceStoreRosterSavePort port(store.rosterRepository());
    return Application::RosterSaveUseCase::execute(
        request, port, caseInsensitiveEquals);
}

Domain::Result<bool> rosterColumnHasTailDataAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId,
    const std::u16string& columnName
    )
{
    const auto legacyId = legacyClassId(classId);
    if (!legacyId)
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<Domain::Result<bool>>(opened);
    }

    QSqlQuery columns(store.database());
    columns.prepare(QStringLiteral(
        "SELECT name FROM roster_columns WHERE class_id=? ORDER BY position, id"));
    columns.addBindValue(*legacyId);
    if (!columns.exec())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The roster columns could not be checked."));
    }

    const std::u16string normalizedRequestedName =
        Application::normalizeRosterCustomColumnName(
            columnName, caseInsensitiveEquals);
    if (normalizedRequestedName.empty())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Validation,
            "The roster column name cannot be mapped unambiguously."));
    }

    int sourceColumnIndex = -1;
    int matchingColumnCount = 0;
    int orderedColumnIndex = 0;
    while (columns.next())
    {
        const std::u16string normalizedStoredName =
            Application::normalizeRosterCustomColumnName(
                columns.value(0).toString().toStdU16String(),
                caseInsensitiveEquals);
        if (!normalizedStoredName.empty()
            && caseInsensitiveEquals(normalizedStoredName, normalizedRequestedName))
        {
            sourceColumnIndex = orderedColumnIndex;
            ++matchingColumnCount;
        }
        ++orderedColumnIndex;
    }
    if (columns.lastError().isValid())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The roster columns could not be checked."));
    }
    columns.finish();
    if (matchingColumnCount != 1 || sourceColumnIndex < 0)
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Validation,
            "The roster column name cannot be mapped unambiguously."));
    }

    QSqlQuery tail(store.database());
    tail.prepare(QStringLiteral(
        "SELECT EXISTS(SELECT 1 FROM roster_data "
        "WHERE class_id=? AND row_index>=? AND col_index=?)"));
    tail.addBindValue(*legacyId);
    tail.addBindValue(static_cast<int>(Application::RosterModeledRowCount));
    tail.addBindValue(sourceColumnIndex);
    if (!tail.exec() || !tail.next())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The stored roster rows could not be checked."));
    }
    const bool hasTailData = tail.value(0).toBool();
    if (tail.lastError().isValid())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The stored roster rows could not be checked."));
    }
    return Domain::Result<bool>::success(hasTailData);
}

} // namespace ClassMngr::Next::Platform
