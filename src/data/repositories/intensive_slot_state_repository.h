#pragma once

#include "core/result.h"
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

    [[nodiscard]] Status saveIntensiveSlotState(
        const QString& day,
        const QString& startTime,
        const QString& state,
        const QString& defaultState = QStringLiteral("essay")
        );

private:
    QSqlDatabase& m_database;
};
