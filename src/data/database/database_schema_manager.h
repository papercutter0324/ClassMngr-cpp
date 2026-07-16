#pragma once

class QSqlDatabase;

class DatabaseSchemaManager
{
public:
    static void ensureSchema(QSqlDatabase& database);
};
