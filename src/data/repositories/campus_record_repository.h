#pragma once

#include "domain/models/campus.h"

#include <QList>
#include <QSqlDatabase>

class CampusRecordRepository
{
public:
    explicit CampusRecordRepository(
        QSqlDatabase& database
        );

    int saveCampus(
        const CampusRecord& campus
        );

    CampusRecord getCampus(
        int campusId
        );

    QList<CampusRecord> getAllCampuses();

    void deleteCampus(
        int campusId
        );

private:
    QSqlDatabase& m_database;
};
