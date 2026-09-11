#pragma once

#include "core/result.h"
#include "domain/models/campus.h"

#include <QList>
#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class CampusRecordRepository
{
public:
    explicit CampusRecordRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
    explicit CampusRecordRepository(
        QSqlDatabase& database
        );
    ~CampusRecordRepository();

    [[nodiscard]] Result<int> saveCampus(
        const CampusRecord& campus
        );

    [[nodiscard]] Result<CampusRecord> getCampus(
        int campusId
        );

    [[nodiscard]] Result<QList<CampusRecord>> getAllCampuses();

    [[nodiscard]] Status deleteCampus(
        int campusId
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation,
        const QString& campusContext = {}
        );

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
