#pragma once

#include <QSqlError>
#include <QString>

#include <expected>

class QSqlQuery;

namespace SqlQueryUtils
{

struct ExecutionError
{
    QString action;
    QString queryText;
    QSqlError sqlError;
    QString driverError;
    QString databaseError;
    QString nativeErrorCode;
    QString recordIdentity;

    [[nodiscard]] QString userMessage() const;
};

using ExecutionResult = std::expected<void, ExecutionError>;

[[nodiscard]] ExecutionError errorFor(
    const QSqlQuery& query,
    const QString& action,
    const QString& queryText = {},
    const QString& recordIdentity = {}
    );

[[nodiscard]] ExecutionResult execute(
    QSqlQuery& query,
    const QString& action
    );

[[nodiscard]] ExecutionResult executePrepared(
    QSqlQuery& query,
    const QString& action,
    const QString& recordIdentity = {}
    );

[[nodiscard]] ExecutionResult execute(
    QSqlQuery& query,
    const QString& queryText,
    const QString& action,
    const QString& recordIdentity = {}
    );

} // namespace SqlQueryUtils
