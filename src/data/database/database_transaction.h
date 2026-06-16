#pragma once

#include <QSqlDatabase>

class DatabaseTransaction
{
public:
    explicit DatabaseTransaction(
        QSqlDatabase& database
        );

    ~DatabaseTransaction();

    bool started() const;

    bool commit();

    void rollback();

private:
    QSqlDatabase& m_database;
    bool m_started = false;
    bool m_finished = false;
};
