#pragma once

#include "core/application_services.h"
#include "core/utils/student_name_utils.h"
#include "data/database/database_session.h"
#include "data/database/sql_query_utils.h"
#include "next/application/class_analytics_dashboard_read_port.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

#include <array>
#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassAnalyticsDashboardReadPort final
    : public Application::ClassAnalyticsDashboardReadPort
    , public Application::ClassAnalyticsNameSemanticsPort
{
public:
    explicit ApplicationServicesClassAnalyticsDashboardReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    explicit ApplicationServicesClassAnalyticsDashboardReadPort(
        ApplicationServices& services
        ) noexcept
        : ApplicationServicesClassAnalyticsDashboardReadPort(&services)
    {
    }

    [[nodiscard]] Domain::Result<Application::ClassAnalyticsRosterNames>
    readRosterNames(const Domain::ClassId& classId) const override
    {
        const std::optional<int> legacyId = legacyClassId(classId.value());
        if (!legacyId)
        {
            return failure<Application::ClassAnalyticsRosterNames>(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer.");
        }

        QSqlDatabase database;
        if (!activeDatabase(database))
        {
            return unavailableFailure<Application::ClassAnalyticsRosterNames>();
        }

        try
        {
            Application::ClassAnalyticsRosterNames roster;
            QSqlQuery columnCountQuery(database);
            columnCountQuery.prepare(R"(
                SELECT COUNT(*)
                FROM roster_columns
                WHERE class_id=?
            )");
            columnCountQuery.addBindValue(*legacyId);
            auto executed = SqlQueryUtils::executePrepared(
                columnCountQuery, QStringLiteral("Loading roster column count"),
                classIdentity(*legacyId));
            if (!executed)
            {
                return sqlFailure<Application::ClassAnalyticsRosterNames>(
                    executed.error().userMessage());
            }
            if (!columnCountQuery.next())
            {
                return technicalFailure<Application::ClassAnalyticsRosterNames>(
                    "Roster column count could not be read.");
            }
            const int columnCount = columnCountQuery.value(0).toInt();
            if (columnCount <= 0)
            {
                return Domain::Result<
                    Application::ClassAnalyticsRosterNames>::success(
                        std::move(roster));
            }

            int englishColumn = -1;
            int koreanColumn = -1;
            QSqlQuery nameColumnsQuery(database);
            nameColumnsQuery.prepare(R"(
                SELECT selected.name,
                       (
                           SELECT COUNT(*) - 1
                           FROM roster_columns preceding
                           WHERE preceding.class_id=selected.class_id
                             AND (
                                 preceding.position < selected.position
                                 OR (preceding.position = selected.position
                                     AND preceding.id <= selected.id)
                             )
                       ) AS column_index
                FROM roster_columns selected
                WHERE selected.class_id=?
                  AND selected.name IN ('English', 'Korean')
                ORDER BY selected.position, selected.id
            )");
            nameColumnsQuery.addBindValue(*legacyId);
            executed = SqlQueryUtils::executePrepared(
                nameColumnsQuery, QStringLiteral("Loading roster name columns"),
                classIdentity(*legacyId));
            if (!executed)
            {
                return sqlFailure<Application::ClassAnalyticsRosterNames>(
                    executed.error().userMessage());
            }
            while (nameColumnsQuery.next())
            {
                const QString name = nameColumnsQuery.value("name").toString();
                const int column =
                    nameColumnsQuery.value("column_index").toInt();
                if (name == QStringLiteral("English") && englishColumn < 0)
                {
                    englishColumn = column;
                }
                else if (name == QStringLiteral("Korean") && koreanColumn < 0)
                {
                    koreanColumn = column;
                }
            }
            roster.hasEnglishColumn = englishColumn >= 0;
            roster.hasKoreanColumn = koreanColumn >= 0;

            QSqlQuery maximumRowQuery(database);
            maximumRowQuery.prepare(R"(
                SELECT MAX(row_index)
                FROM roster_data
                WHERE class_id=?
                  AND row_index>=0
                  AND col_index>=0
                  AND col_index<?
            )");
            maximumRowQuery.addBindValue(*legacyId);
            maximumRowQuery.addBindValue(columnCount);
            executed = SqlQueryUtils::executePrepared(
                maximumRowQuery, QStringLiteral("Sizing roster rows"),
                classIdentity(*legacyId));
            if (!executed)
            {
                return sqlFailure<Application::ClassAnalyticsRosterNames>(
                    executed.error().userMessage());
            }
            if (!maximumRowQuery.next())
            {
                return technicalFailure<Application::ClassAnalyticsRosterNames>(
                    "Roster row count could not be read.");
            }
            const QVariant maximumRow = maximumRowQuery.value(0);
            roster.rowCount = maximumRow.isNull()
                ? 0
                : static_cast<std::size_t>(maximumRow.toInt() + 1);
            roster.englishNames.resize(roster.rowCount);
            roster.koreanNames.resize(roster.rowCount);
            if (roster.rowCount == 0
                || (!roster.hasEnglishColumn && !roster.hasKoreanColumn))
            {
                return Domain::Result<
                    Application::ClassAnalyticsRosterNames>::success(
                        std::move(roster));
            }

            QSqlQuery namesQuery(database);
            namesQuery.prepare(R"(
                SELECT row_index, col_index, value
                FROM roster_data
                WHERE class_id=?
                  AND row_index>=0
                  AND row_index<?
                  AND (col_index=? OR col_index=?)
                ORDER BY row_index, col_index
            )");
            namesQuery.addBindValue(*legacyId);
            namesQuery.addBindValue(static_cast<qulonglong>(roster.rowCount));
            namesQuery.addBindValue(englishColumn);
            namesQuery.addBindValue(koreanColumn);
            executed = SqlQueryUtils::executePrepared(
                namesQuery, QStringLiteral("Loading roster student names"),
                classIdentity(*legacyId));
            if (!executed)
            {
                return sqlFailure<Application::ClassAnalyticsRosterNames>(
                    executed.error().userMessage());
            }
            while (namesQuery.next())
            {
                const int row = namesQuery.value("row_index").toInt();
                const int column = namesQuery.value("col_index").toInt();
                if (row < 0
                    || static_cast<std::size_t>(row) >= roster.rowCount)
                {
                    continue;
                }
                const std::u16string value = namesQuery.value("value")
                    .toString().toStdU16String();
                if (column == englishColumn)
                {
                    roster.englishNames[static_cast<std::size_t>(row)] = value;
                }
                if (column == koreanColumn)
                {
                    roster.koreanNames[static_cast<std::size_t>(row)] = value;
                }
            }
            return Domain::Result<
                Application::ClassAnalyticsRosterNames>::success(
                    std::move(roster));
        }
        catch (const std::exception&)
        {
            return technicalFailure<Application::ClassAnalyticsRosterNames>(
                "Roster names could not be loaded.");
        }
        catch (...)
        {
            return technicalFailure<Application::ClassAnalyticsRosterNames>(
                "Roster names could not be loaded.");
        }
    }

    [[nodiscard]] Domain::Result<Application::ClassAnalyticsEvaluationBatch>
    readEvaluationBatch(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyId = legacyClassId(classId.value());
        if (!legacyId)
        {
            return failure<Application::ClassAnalyticsEvaluationBatch>(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer.");
        }

        QSqlDatabase database;
        if (!activeDatabase(database))
        {
            return unavailableFailure<Application::ClassAnalyticsEvaluationBatch>();
        }

        try
        {
            QSqlQuery rowsQuery(database);
            rowsQuery.prepare(R"(
                WITH requested(evaluation_name, request_order) AS (
                    VALUES (?, 0), (?, 1), (?, 2), (?, 3)
                )
                SELECT requested.request_order AS request_order,
                       data.evaluation_id AS data_evaluation_id,
                       data.row_index AS row_index,
                       data.col_1 AS col_1,
                       data.col_2 AS col_2,
                       data.col_3 AS col_3,
                       data.col_4 AS col_4,
                       data.col_5 AS col_5,
                       data.col_6 AS col_6,
                       data.col_7 AS col_7,
                       data.col_8 AS col_8
                FROM requested
                LEFT JOIN speaking_evaluations evaluation
                    ON evaluation.class_id=?
                   AND evaluation.evaluation_name=requested.evaluation_name
                LEFT JOIN speaking_eval_data data
                    ON data.evaluation_id=evaluation.id
                ORDER BY requested.request_order, data.row_index
            )");
            for (const std::u16string_view name :
                 Application::kClassAnalyticsEvaluationNames)
            {
                rowsQuery.addBindValue(
                    QString::fromStdU16String(std::u16string(name))
                    );
            }
            rowsQuery.addBindValue(*legacyId);
            const auto executed = SqlQueryUtils::executePrepared(
                rowsQuery,
                QStringLiteral("Loading Class Analytics evaluations"),
                classIdentity(*legacyId));
            if (!executed)
            {
                return sqlFailure<Application::ClassAnalyticsEvaluationBatch>(
                    executed.error().userMessage());
            }

            Application::ClassAnalyticsEvaluationBatch batch;
            std::array<bool, Application::kClassAnalyticsEvaluationNames.size()>
                seenRequests{};
            while (rowsQuery.next())
            {
                bool requestOrderIsValid = false;
                const int requestOrder = rowsQuery.value("request_order")
                    .toInt(&requestOrderIsValid);
                if (!requestOrderIsValid
                    || requestOrder < 0
                    || requestOrder >= static_cast<int>(batch.size()))
                {
                    return technicalFailure<
                        Application::ClassAnalyticsEvaluationBatch>(
                            "Class Analytics returned an invalid evaluation order.");
                }
                seenRequests[static_cast<std::size_t>(requestOrder)] = true;

                // LEFT JOIN contributes one placeholder row for a missing
                // evaluation or an evaluation with no stored score rows.
                if (rowsQuery.value("data_evaluation_id").isNull())
                {
                    continue;
                }

                Application::ClassAnalyticsEvaluationRow row;
                row.englishName = rowsQuery.value("col_1")
                    .toString().toStdU16String();
                row.koreanName = rowsQuery.value("col_2")
                    .toString().toStdU16String();
                for (std::size_t score = 0; score < row.scores.size(); ++score)
                {
                    row.scores[score] = rowsQuery.value(
                        QStringLiteral("col_%1").arg(score + 3))
                        .toString().toStdU16String();
                }
                batch[static_cast<std::size_t>(requestOrder)].push_back(
                    std::move(row));
            }
            if (rowsQuery.lastError().isValid())
            {
                return sqlFailure<Application::ClassAnalyticsEvaluationBatch>(
                    rowsQuery.lastError().text());
            }
            for (const bool seen : seenRequests)
            {
                if (!seen)
                {
                    return technicalFailure<
                        Application::ClassAnalyticsEvaluationBatch>(
                            "Class Analytics returned an incomplete evaluation batch.");
                }
            }
            return Domain::Result<Application::ClassAnalyticsEvaluationBatch>::
                success(std::move(batch));
        }
        catch (const std::exception&)
        {
            return technicalFailure<Application::ClassAnalyticsEvaluationBatch>(
                "Class Analytics evaluations could not be loaded.");
        }
        catch (...)
        {
            return technicalFailure<Application::ClassAnalyticsEvaluationBatch>(
                "Class Analytics evaluations could not be loaded.");
        }
    }

    [[nodiscard]] std::u16string trimmed(
        const std::u16string_view value
        ) const override
    {
        return QString::fromStdU16String(std::u16string(value))
            .trimmed().toStdU16String();
    }

    [[nodiscard]] std::u16string normalizedEnglishIdentity(
        const std::u16string_view value
        ) const override
    {
        return StudentNameUtils::normalizeEnglishName(
            QString::fromStdU16String(std::u16string(value)))
            .toCaseFolded().toStdU16String();
    }

    [[nodiscard]] std::u16string baseKoreanName(
        const std::u16string_view value
        ) const override
    {
        return StudentNameUtils::baseKoreanName(
            QString::fromStdU16String(std::u16string(value)))
            .toStdU16String();
    }

    [[nodiscard]] bool equalsCaseInsensitive(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        return QString::compare(
            QString::fromStdU16String(std::u16string(left)),
            QString::fromStdU16String(std::u16string(right)),
            Qt::CaseInsensitive) == 0;
    }

    [[nodiscard]] int compareEnglishNames(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        return QString::localeAwareCompare(
            QString::fromStdU16String(std::u16string(left)),
            QString::fromStdU16String(std::u16string(right)));
    }

private:
    [[nodiscard]] bool activeDatabase(QSqlDatabase& database) const
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen()) return false;
        database = session->database();
        return database.isValid() && database.isOpen();
    }

    [[nodiscard]] static std::optional<int> legacyClassId(
        const std::string& value
        )
    {
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0
            || std::to_string(parsed) != value)
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static QString classIdentity(const int classId)
    {
        return QStringLiteral("class id %1").arg(classId);
    }

    template<typename Value>
    [[nodiscard]] static Domain::Result<Value> failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Domain::Result<Value>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::InvalidInput
        });
    }

    template<typename Value>
    [[nodiscard]] static Domain::Result<Value> technicalFailure(
        std::string message
        )
    {
        return failure<Value>(Domain::ErrorCode::Technical, std::move(message));
    }

    template<typename Value>
    [[nodiscard]] static Domain::Result<Value> sqlFailure(
        const QString& message
        )
    {
        return technicalFailure<Value>(message.toStdString());
    }

    template<typename Value>
    [[nodiscard]] static Domain::Result<Value> unavailableFailure()
    {
        return failure<Value>(
            Domain::ErrorCode::NotFound,
            "The active class analytics database session is unavailable.");
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
