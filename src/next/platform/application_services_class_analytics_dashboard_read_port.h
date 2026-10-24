#pragma once

#include "core/application_services.h"
#include "core/utils/student_name_utils.h"
#include "data/database/workspace_store.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "next/application/class_analytics_dashboard_read_port.h"

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

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        RosterRepository* const repository =
            store && store->isOpen() ? store->rosterRepository() : nullptr;
        if (!repository)
        {
            return unavailableFailure<Application::ClassAnalyticsRosterNames>();
        }

        try
        {
            Application::ClassAnalyticsRosterNames roster;
            const Status loaded = repository->forEachClassAnalyticsRosterNameCell(
                *legacyId,
                [&](const RosterRepository::ClassAnalyticsRosterNamesMetadata& metadata)
                {
                    roster.rowCount = Application::ClassAnalyticsRosterRowCount{
                        metadata.rowCount
                    };
                    roster.hasEnglishColumn = metadata.hasEnglishColumn;
                    roster.hasKoreanColumn = metadata.hasKoreanColumn;
                },
                [&](const RosterRepository::ClassAnalyticsRosterNameCell& cell)
                {
                    auto& names = cell.isEnglishName
                        ? roster.englishNames : roster.koreanNames;
                    names.push_back(cell.value.toStdU16String());
                });
            if (!loaded)
            {
                return repositoryFailure<Application::ClassAnalyticsRosterNames>(
                    loaded.error());
            }
            return Domain::Result<Application::ClassAnalyticsRosterNames>::success(
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

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        SpeakingEvalRepository* const repository =
            store && store->isOpen() ? store->speakingEvalRepository() : nullptr;
        if (!repository)
        {
            return unavailableFailure<Application::ClassAnalyticsEvaluationBatch>();
        }

        try
        {
            std::array<QString, Application::kClassAnalyticsEvaluationNames.size()>
                evaluationNames;
            for (std::size_t index = 0; index < evaluationNames.size(); ++index)
            {
                evaluationNames[index] = QString::fromStdU16String(
                    std::u16string(Application::kClassAnalyticsEvaluationNames[index]));
            }
            Application::ClassAnalyticsEvaluationBatch batch;
            const Status loaded = repository->forEachClassAnalyticsEvaluationRow(
                *legacyId, evaluationNames,
                [&](const SpeakingEvalRepository::ClassAnalyticsEvaluationRow& source)
                {
                    Application::ClassAnalyticsEvaluationRow row;
                    row.englishName = source.englishName.toStdU16String();
                    row.koreanName = source.koreanName.toStdU16String();
                    for (std::size_t score = 0; score < row.scores.size(); ++score)
                    {
                        row.scores[score] = source.scores[score].toStdU16String();
                    }
                    batch[source.evaluationIndex].push_back(std::move(row));
                });
            if (!loaded)
            {
                return repositoryFailure<Application::ClassAnalyticsEvaluationBatch>(
                    loaded.error());
            }
            return Domain::Result<Application::ClassAnalyticsEvaluationBatch>::success(
                std::move(batch));
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
    [[nodiscard]] static Domain::Result<Value> repositoryFailure(
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
