#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseTransaction
{
public:
    enum class Mode
    {
        Default,
        Immediate
    };

    explicit DatabaseTransaction(
        QSqlDatabase& database,
        Mode mode = Mode::Default
        );

    ~DatabaseTransaction();

    bool started() const;

    const QString& startError() const;

    bool commit();

    void rollback();

    bool rollbackChecked();

private:
    QSqlDatabase& m_database;
    bool m_started = false;
    bool m_finished = false;
    QString m_startError;
};
