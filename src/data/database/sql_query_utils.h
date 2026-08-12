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

    [[nodiscard]] QString userMessage() const;
};

using ExecutionResult = std::expected<void, ExecutionError>;

[[nodiscard]] ExecutionError errorFor(
    const QSqlQuery& query,
    const QString& action,
    const QString& queryText = {}
    );

[[nodiscard]] ExecutionResult execute(
    QSqlQuery& query,
    const QString& action
    );

[[nodiscard]] ExecutionResult execute(
    QSqlQuery& query,
    const QString& queryText,
    const QString& action
    );

} // namespace SqlQueryUtils
