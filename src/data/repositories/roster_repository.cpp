#include "roster_repository.h"

#include "classmngr/engine/open_database.h"
#include "classmngr/engine/roster_service.h"
#include "classmngr/engine/sqlite_database.h"

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using EngineError = classmngr::engine::Error;
using EngineRoster = classmngr::engine::Roster;
using EngineRosterService = classmngr::engine::RosterService;

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

EngineRoster toEngineRoster(const Roster& source)
{
    EngineRoster result;
    result.columns.reserve(static_cast<std::size_t>(source.columns.size()));
    for (const QString& column : source.columns)
    {
        result.columns.push_back(toUtf8(column));
    }

    result.columnWidths.reserve(
        static_cast<std::size_t>(source.columnWidths.size())
        );
    for (const int width : source.columnWidths)
    {
        result.columnWidths.push_back(width);
    }

    result.rows.reserve(static_cast<std::size_t>(source.rows.size()));
    for (const QStringList& sourceRow : source.rows)
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

Roster fromEngineRoster(const EngineRoster& source)
{
    Roster result;
    result.columns.reserve(static_cast<qsizetype>(source.columns.size()));
    for (const std::string& column : source.columns)
    {
        result.columns.append(fromUtf8(column));
    }

    result.columnWidths.reserve(
        static_cast<qsizetype>(source.columnWidths.size())
        );
    for (const int width : source.columnWidths)
    {
        result.columnWidths.append(width);
    }

    result.rows.reserve(static_cast<qsizetype>(source.rows.size()));
    for (const std::vector<std::string>& sourceRow : source.rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::string& value : sourceRow)
        {
            row.append(fromUtf8(value));
        }
        result.rows.append(std::move(row));
    }

    return result;
}

QString localizedOperation(
    const QString& detail,
    const QString& fallback
    )
{
    struct Operation
    {
        const char* text;
    };

    constexpr Operation operations[] = {
        {"Loading roster columns"},
        {"Loading roster data"},
        {"Deleting roster columns"},
        {"Deleting roster data"},
        {"Inserting roster column"},
        {"Inserting roster data"},
        {"Starting roster save transaction"},
        {"Committing roster save"},
        {"Starting roster batch save transaction"},
        {"Committing roster batch save"},
        {"Loading roster"},
        {"Saving roster"}
    };

    for (const Operation& operation : operations)
    {
        if (detail.startsWith(QString::fromLatin1(operation.text)))
        {
            return QObject::tr(operation.text);
        }
    }

    return fallback;
}

QString engineFailure(
    const QString& fallbackOperation,
    int classId,
    const EngineError& error
    )
{
    const QString detail = fromUtf8(error.message);
    QString message = QObject::tr("%1 failed")
        .arg(localizedOperation(detail, fallbackOperation));

    if (!detail.contains(QStringLiteral("class id ")))
    {
        message += QObject::tr(" for class id %1").arg(classId);
    }

    if (!detail.isEmpty())
    {
        message += QStringLiteral(": ") + detail;
    }

    return message;
}

QString invalidClassIdError(
    const QString& operation,
    int classId
    )
{
    return QObject::tr("%1 failed: invalid class id %2.")
        .arg(operation)
        .arg(classId);
}
} // namespace

RosterRepository::RosterRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

RosterRepository::RosterRepository(
    QSqlDatabase& database
    )
    : RosterRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

RosterRepository::~RosterRepository() = default;

Status RosterRepository::ensureEngineDatabase(
    const QString& operation,
    int classId
    )
{
    if (!m_compatibilityDatabaseWasOpen)
    {
        return std::unexpected(
            QObject::tr("%1 failed for class id %2: No Teacher Profile is open.")
                .arg(operation)
                .arg(classId)
            );
    }

    const QString databasePath = m_databasePath;
    if (databasePath.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr("%1 failed for class id %2: No database path is available.")
                .arg(operation)
                .arg(classId)
            );
    }

    if (m_engineDatabase
        && m_engineDatabase->isOpen()
        && m_engineDatabasePath == databasePath)
    {
        return {};
    }

    m_engineDatabase.reset();
    m_engineDatabasePath.clear();

    const std::string encodedPath = toUtf8(databasePath);
    auto opened = classmngr::engine::OpenDatabase::execute(encodedPath);
    if (!opened || *opened == nullptr)
    {
        if (!opened)
        {
            return std::unexpected(
                engineFailure(operation, classId, opened.error())
                );
        }

        return std::unexpected(
            QObject::tr("%1 failed for class id %2: The engine database could not be opened.")
                .arg(operation)
                .arg(classId)
            );
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

Status RosterRepository::saveRoster(
    int classId,
    const Roster& roster
    )
{
    const QString operation = QObject::tr("Saving roster");
    if (classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, classId));
    }

    const Status engineReady = ensureEngineDatabase(operation, classId);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineRosterService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.save(
        classId,
        toEngineRoster(roster)
        );
    if (!saved)
    {
        return std::unexpected(engineFailure(operation, classId, saved.error()));
    }

    return {};
}

Status RosterRepository::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    )
{
    if (rosters.isEmpty())
    {
        return {};
    }

    const QString operation = QObject::tr("Saving roster batch");
    for (const auto& roster : rosters)
    {
        if (roster.first <= 0)
        {
            return std::unexpected(
                invalidClassIdError(
                    QObject::tr("Saving roster"),
                    roster.first
                    )
                );
        }
    }

    const Status engineReady = ensureEngineDatabase(
        operation,
        rosters.first().first
        );
    if (!engineReady)
    {
        return engineReady;
    }

    std::vector<std::pair<int, EngineRoster>> portableRosters;
    portableRosters.reserve(static_cast<std::size_t>(rosters.size()));
    for (const auto& roster : rosters)
    {
        portableRosters.emplace_back(
            roster.first,
            toEngineRoster(roster.second)
            );
    }

    EngineRosterService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.saveBatch(portableRosters);
    if (!saved)
    {
        return std::unexpected(
            engineFailure(operation, rosters.first().first, saved.error())
            );
    }

    return {};
}

Result<Roster> RosterRepository::loadRoster(
    int classId
    )
{
    const QString operation = QObject::tr("Loading roster");
    if (classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, classId));
    }

    const Status engineReady = ensureEngineDatabase(
        QObject::tr("Loading roster columns"),
        classId
        );
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineRosterService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineRoster> loaded = service.load(classId);
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(
                QObject::tr("Loading roster columns"),
                classId,
                loaded.error()
                )
            );
    }

    return fromEngineRoster(*loaded);
}

Result<int> RosterRepository::getRosterStudentCount(
    int classId
    )
{
    const Result<Roster> roster = loadRoster(classId);
    if (!roster)
    {
        return std::unexpected(roster.error());
    }

    const int englishColumn =
        roster->columns.indexOf(
            QStringLiteral("English")
            );

    const int koreanColumn =
        roster->columns.indexOf(
            QStringLiteral("Korean")
            );

    if (englishColumn < 0 && koreanColumn < 0)
    {
        return 0;
    }

    int count = 0;

    for (const QStringList& row : roster->rows)
    {
        const bool hasEnglish =
            englishColumn >= 0
            && englishColumn < row.size()
            && !row[englishColumn].trimmed().isEmpty();

        const bool hasKorean =
            koreanColumn >= 0
            && koreanColumn < row.size()
            && !row[koreanColumn].trimmed().isEmpty();

        if (hasEnglish || hasKorean)
        {
            ++count;
        }
    }

    return count;
}
