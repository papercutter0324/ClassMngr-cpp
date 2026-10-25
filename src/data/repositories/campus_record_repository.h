#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/campus.h"

#include <QList>
#include <QSqlDatabase>

class CampusRecordRepository
{
public:
    explicit CampusRecordRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryResult<int> saveCampus(
        const CampusRecord& campus
        );

    [[nodiscard]] RepositoryResult<CampusRecord> getCampus(
        int campusId
        );

    [[nodiscard]] RepositoryResult<QList<CampusRecord>> getAllCampuses();

    [[nodiscard]] RepositoryStatus deleteCampus(
        int campusId
        );

private:
    QSqlDatabase& m_database;
};
