#include "database_transaction.h"

#include <QSqlError>
#include <QSqlQuery>

DatabaseTransaction::DatabaseTransaction(
    QSqlDatabase& database,
    const Mode mode
    )
    : m_database(database)
{
    if (mode == Mode::Immediate)
    {
        QSqlQuery query(m_database);
        m_started = query.exec(QStringLiteral("BEGIN IMMEDIATE"));
        if (!m_started)
        {
            m_startError = query.lastError().text();
        }
        return;
    }

    m_started = m_database.transaction();
    if (!m_started)
    {
        m_startError = m_database.lastError().text();
    }
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

const QString& DatabaseTransaction::startError() const
{
    return m_startError;
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
    (void)rollbackChecked();
}

bool DatabaseTransaction::rollbackChecked()
{
    if (!m_started || m_finished)
    {
        return true;
    }

    const bool rolledBack = m_database.rollback();
    m_finished = true;
    return rolledBack;
}
