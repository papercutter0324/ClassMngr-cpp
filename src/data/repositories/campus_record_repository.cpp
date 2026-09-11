#include "campus_record_repository.h"

#include "classmngr/engine/campus_record_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"

#include <QByteArray>
#include <QObject>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using EngineCampusRecord = classmngr::engine::CampusRecord;
using EngineCampusRecordService = classmngr::engine::CampusRecordService;
using EngineError = classmngr::engine::Error;

std::string toUtf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(
    std::string_view value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

EngineCampusRecord toEngineCampus(
    const CampusRecord& source
    )
{
    EngineCampusRecord result;
    result.id = source.id;
    result.name = toUtf8(source.name);
    result.buildingName = toUtf8(source.buildingName);
    result.address = toUtf8(source.address);
    result.phoneNumber = toUtf8(source.phoneNumber);
    result.officeNumber = toUtf8(source.officeNumber);
    result.transitSteps = toUtf8(source.transitSteps);
    result.arrivalInfo = toUtf8(source.arrivalInfo);
    result.imagePath = toUtf8(source.imagePath);
    result.officeWifi = toUtf8(source.officeWifi);
    result.officeWifiPassword = toUtf8(source.officeWifiPassword);
    result.printerName = toUtf8(source.printerName);
    result.printerSteps = toUtf8(source.printerSteps);
    result.photocopierCode = toUtf8(source.photocopierCode);
    result.housingLocations = toUtf8(source.housingLocations);
    return result;
}

CampusRecord fromEngineCampus(
    const EngineCampusRecord& source
    )
{
    CampusRecord result;
    result.id = source.id;
    result.name = fromUtf8(source.name);
    result.buildingName = fromUtf8(source.buildingName);
    result.address = fromUtf8(source.address);
    result.phoneNumber = fromUtf8(source.phoneNumber);
    result.officeNumber = fromUtf8(source.officeNumber);
    result.transitSteps = fromUtf8(source.transitSteps);
    result.arrivalInfo = fromUtf8(source.arrivalInfo);
    result.imagePath = fromUtf8(source.imagePath);
    result.officeWifi = fromUtf8(source.officeWifi);
    result.officeWifiPassword = fromUtf8(source.officeWifiPassword);
    result.printerName = fromUtf8(source.printerName);
    result.printerSteps = fromUtf8(source.printerSteps);
    result.photocopierCode = fromUtf8(source.photocopierCode);
    result.housingLocations = fromUtf8(source.housingLocations);
    return result;
}

QString campusIdentity(
    const CampusRecord& campus
    )
{
    if (campus.id > 0)
    {
        return QObject::tr("campus id %1").arg(campus.id);
    }

    return QObject::tr("campus '%1'").arg(campus.name.trimmed());
}

QString campusIdentity(
    int campusId
    )
{
    return QObject::tr("campus id %1").arg(campusId);
}

QString invalidCampusIdError(
    const QString& operation,
    int campusId
    )
{
    return QObject::tr("%1 failed: invalid campus id %2.")
        .arg(operation)
        .arg(campusId);
}

QString operationFailure(
    const QString& operation,
    const QString& campusContext,
    const QString& detail
    )
{
    QString message = QObject::tr("%1 failed").arg(operation);

    if (!campusContext.trimmed().isEmpty())
    {
        message += QObject::tr(" for %1").arg(campusContext);
    }

    const QString trimmedDetail = detail.trimmed();
    if (!trimmedDetail.isEmpty())
    {
        message += QStringLiteral(": ") + trimmedDetail;
    }

    return message;
}

QString engineErrorDetail(
    const EngineError& error
    )
{
    if (error.code == classmngr::engine::ErrorCode::NotFound)
    {
        return QObject::tr("no matching record exists.");
    }

    const QString detail = fromUtf8(error.message);
    if (!detail.trimmed().isEmpty())
    {
        return detail;
    }

    return QObject::tr("The engine reported a %1 error.")
        .arg(fromUtf8(classmngr::engine::errorCodeName(error.code)));
}

QString engineFailure(
    const QString& operation,
    const QString& campusContext,
    const EngineError& error
    )
{
    return operationFailure(
        operation,
        campusContext,
        engineErrorDetail(error)
        );
}
} // namespace

CampusRecordRepository::CampusRecordRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

CampusRecordRepository::CampusRecordRepository(
    QSqlDatabase& database
    )
    : CampusRecordRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

CampusRecordRepository::~CampusRecordRepository() = default;

Status CampusRecordRepository::ensureEngineDatabase(
    const QString& operation,
    const QString& campusContext
    )
{
    if (!m_compatibilityDatabaseWasOpen)
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                campusContext,
                QObject::tr("No Teacher Profile is open.")
                )
            );
    }

    const QString databasePath = m_databasePath;
    if (databasePath.trimmed().isEmpty()
        || databasePath.trimmed() == QStringLiteral(":memory:"))
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                campusContext,
                QObject::tr("No database path is available.")
                )
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

    auto opened = classmngr::engine::OpenDatabase::execute(
        toUtf8(databasePath)
        );
    if (!opened)
    {
        return std::unexpected(
            engineFailure(operation, campusContext, opened.error())
            );
    }
    if (*opened == nullptr)
    {
        return std::unexpected(
            operationFailure(
                operation,
                campusContext,
                QObject::tr("The engine database could not be opened.")
                )
            );
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

Result<int> CampusRecordRepository::saveCampus(
    const CampusRecord& campus
    )
{
    const QString operation = campus.id > 0
        ? QObject::tr("Updating campus")
        : QObject::tr("Creating campus");
    const QString identity = campusIdentity(campus);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCampusRecordService service(*m_engineDatabase);
    const classmngr::engine::Result<int> saved = service.save(
        toEngineCampus(campus)
        );
    if (!saved)
    {
        return std::unexpected(
            engineFailure(operation, identity, saved.error())
            );
    }

    return *saved;
}

Result<CampusRecord> CampusRecordRepository::getCampus(
    int campusId
    )
{
    const QString operation = QObject::tr("Loading campus");
    if (campusId <= 0)
    {
        return std::unexpected(invalidCampusIdError(operation, campusId));
    }

    const QString identity = campusIdentity(campusId);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCampusRecordService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineCampusRecord> loaded = service.get(
        campusId
        );
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, identity, loaded.error())
            );
    }

    return fromEngineCampus(*loaded);
}

Result<QList<CampusRecord>> CampusRecordRepository::getAllCampuses()
{
    const QString operation = QObject::tr("Loading campuses");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCampusRecordService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineCampusRecord>> loaded =
        service.list();
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, {}, loaded.error())
            );
    }

    QList<CampusRecord> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineCampusRecord& campus : *loaded)
    {
        result.append(fromEngineCampus(campus));
    }

    return result;
}

Status CampusRecordRepository::deleteCampus(
    int campusId
    )
{
    const QString operation = QObject::tr("Deleting campus");
    if (campusId <= 0)
    {
        return std::unexpected(invalidCampusIdError(operation, campusId));
    }

    const QString identity = campusIdentity(campusId);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineCampusRecordService service(*m_engineDatabase);
    const classmngr::engine::Status deleted = service.remove(campusId);
    if (!deleted)
    {
        return std::unexpected(
            engineFailure(operation, identity, deleted.error())
            );
    }

    return {};
}
