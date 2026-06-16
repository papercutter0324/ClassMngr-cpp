#include "database_transaction.h"

DatabaseTransaction::DatabaseTransaction(
    QSqlDatabase& database
    )
    : m_database(database)
    , m_started(database.transaction())
{
}

DatabaseTransaction::~DatabaseTransaction()
{
    if (m_started && !m_finished)
    {
        m_database.rollback();
    }
}

bool DatabaseTransaction::started() const
{
    return m_started;
}

bool DatabaseTransaction::commit()
{
    if (!m_started)
    {
        return false;
    }

    m_finished =
        m_database.commit();

    return m_finished;
}

void DatabaseTransaction::rollback()
{
    if (!m_started || m_finished)
    {
        return;
    }

    m_database.rollback();
    m_finished = true;
}
