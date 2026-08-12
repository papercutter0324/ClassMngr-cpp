#include "sql_query_utils.h"

#include <QObject>
#include <QSqlQuery>

namespace SqlQueryUtils
{

QString ExecutionError::userMessage() const
{
    return QObject::tr("%1 failed: %2")
        .arg(action, sqlError.text());
}

ExecutionError errorFor(
    const QSqlQuery& query,
    const QString& action,
    const QString& queryText
    )
{
    return {
        action,
        queryText.isEmpty() ? query.lastQuery() : queryText,
        query.lastError()
    };
}

ExecutionResult execute(
    QSqlQuery& query,
    const QString& action
    )
{
    if (query.exec())
    {
        return {};
    }

    return std::unexpected(errorFor(query, action));
}

ExecutionResult execute(
    QSqlQuery& query,
    const QString& queryText,
    const QString& action
    )
{
    if (query.exec(queryText))
    {
        return {};
    }

    return std::unexpected(errorFor(query, action, queryText));
}

} // namespace SqlQueryUtils
