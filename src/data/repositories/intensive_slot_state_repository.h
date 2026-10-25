#pragma once

#include "data/repositories/repository_error.h"
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

    [[nodiscard]] RepositoryResult<QList<IntensiveSlotState>>
        loadIntensiveSlotStates();

    [[nodiscard]] RepositoryStatus saveIntensiveSlotState(
        const QString& day,
        const QString& startTime,
        const QString& state,
        const QString& defaultState = QStringLiteral("essay")
        );

private:
    QSqlDatabase& m_database;
};
