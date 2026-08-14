#pragma once

#include "core/result.h"
#include "domain/models/campus.h"

#include <QList>
#include <QSqlDatabase>

class CampusRecordRepository
{
public:
    explicit CampusRecordRepository(
        QSqlDatabase& database
        );

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
    QSqlDatabase& m_database;
};
