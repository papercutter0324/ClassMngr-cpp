#pragma once

#include "domain/models/intensive_slot_state.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

class IntensiveSlotStateRepository
{
public:
    explicit IntensiveSlotStateRepository(
        QSqlDatabase& database
        );

    QList<IntensiveSlotState> loadIntensiveSlotStates();

    void saveIntensiveSlotState(
        const QString& day,
        const QString& startTime,
        const QString& state
        );

private:
    QSqlDatabase& m_database;
};
