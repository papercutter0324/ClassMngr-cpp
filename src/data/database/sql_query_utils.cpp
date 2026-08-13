#include "sql_query_utils.h"

#include <QObject>
#include <QSqlQuery>

namespace SqlQueryUtils
{

QString ExecutionError::userMessage() const
{
    QString message = QObject::tr("%1 failed").arg(action);

    if (!recordIdentity.trimmed().isEmpty())
    {
        message += QObject::tr(" for %1").arg(recordIdentity);
    }

    const QString errorText = sqlError.text().trimmed();
    if (!errorText.isEmpty())
    {
        message += QStringLiteral(": ") + errorText;
    }

    if (!nativeErrorCode.trimmed().isEmpty())
    {
        message += QObject::tr(" (database error %1)")
            .arg(nativeErrorCode);
    }

    return message;
}

ExecutionError errorFor(
    const QSqlQuery& query,
    const QString& action,
    const QString& queryText,
    const QString& recordIdentity
    )
{
    const QSqlError sqlError = query.lastError();

    return {
        action,
        queryText.isEmpty() ? query.lastQuery() : queryText,
        sqlError,
        sqlError.driverText(),
        sqlError.databaseText(),
        sqlError.nativeErrorCode(),
        recordIdentity
    };
}

ExecutionResult execute(
    QSqlQuery& query,
    const QString& action
    )
{
    return executePrepared(query, action);
}

ExecutionResult executePrepared(
    QSqlQuery& query,
    const QString& action,
    const QString& recordIdentity
    )
{
    if (query.exec())
    {
        return {};
    }

    return std::unexpected(
        errorFor(query, action, {}, recordIdentity)
        );
}

ExecutionResult execute(
    QSqlQuery& query,
    const QString& queryText,
    const QString& action,
    const QString& recordIdentity
    )
{
    if (query.exec(queryText))
    {
        return {};
    }

    return std::unexpected(
        errorFor(query, action, queryText, recordIdentity)
        );
}

} // namespace SqlQueryUtils
