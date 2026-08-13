#pragma once

#include "core/result.h"

class QSqlDatabase;

class DatabaseSchemaManager
{
public:
    [[nodiscard]] static Status ensureSchema(QSqlDatabase& database);
};
