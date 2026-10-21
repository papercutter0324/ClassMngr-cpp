#pragma once

#include "next/application/schedule_view_projection.h"
#include "next/application/sub_prep_weekday.h"
#include "next/domain/domain_types.h"
#include "next/domain/gregorian_date.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class SubPrepPackageRequestValidationReason
{
    None,
    NoOutputAction,
    NoValidSelectedDate,
    NoSelectedClassIds,
    MissingTargetRoot,
    MissingUserName
};

enum class SubPrepGenerationPlanFailureReason
{
    None,
    NoOutputAction,
    NoValidSelectedDate,
    NoSelectedClassIds,
    MissingTargetRoot,
    MissingUserName,
    InvalidSelectedScheduleWeekday,
    InvalidScheduleMode
};

struct SubPrepPackageRequestValidationRequest
{
    bool createFolder = false;
    bool printPaperCopies = false;
    bool hasValidSelectedDate = false;
    bool hasSelectedClassIds = false;
    bool hasNonEmptyTargetRoot = false;
    bool hasNonEmptyUserName = false;
};

struct SubPrepPackageRequestValidationResult
{
    SubPrepPackageRequestValidationReason reason =
        SubPrepPackageRequestValidationReason::None;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return reason == SubPrepPackageRequestValidationReason::None;
    }
};

[[nodiscard]] constexpr SubPrepPackageRequestValidationResult
validateSubPrepPackageRequest(
    const SubPrepPackageRequestValidationRequest& request
    ) noexcept
{
    using Reason = SubPrepPackageRequestValidationReason;

    if (!request.createFolder && !request.printPaperCopies)
    {
        return {Reason::NoOutputAction};
    }

    if (!request.hasValidSelectedDate)
    {
        return {Reason::NoValidSelectedDate};
    }

    if (!request.hasSelectedClassIds)
    {
        return {Reason::NoSelectedClassIds};
    }

    if (request.createFolder && !request.hasNonEmptyTargetRoot)
    {
        return {Reason::MissingTargetRoot};
    }

    if (request.createFolder && !request.hasNonEmptyUserName)
    {
        return {Reason::MissingUserName};
    }

    return {Reason::None};
}

struct SubPrepGenerationPlanRequest final
{
    std::vector<Domain::GregorianDate> selectedDates;
    std::vector<Domain::ClassId> selectedClassIds;
    std::vector<SubPrepWeekday> selectedScheduleWeekdays;
    ScheduleViewMode mode = ScheduleViewMode::Regular;
    bool createFolder = false;
    bool printPaperCopies = false;
    bool openFolderAfterGeneration = false;
    std::vector<std::string> requestedRosterColumns;
    std::string folderLabel;
    bool hasNonEmptyFolderLabel = false;
    bool targetRootReady = false;
    bool replaceExisting = false;
};

struct SubPrepFolderOutputPlan final
{
    std::string folderLabel;
    bool targetRootReady = false;
    bool replaceExisting = false;
    bool openAfterGeneration = false;

    friend bool operator==(
        const SubPrepFolderOutputPlan&,
        const SubPrepFolderOutputPlan&
        ) = default;
};

struct SubPrepOutputPlan final
{
    bool createFolder = false;
    bool printPaperCopies = false;
    std::optional<SubPrepFolderOutputPlan> folder;

    friend bool operator==(
        const SubPrepOutputPlan&,
        const SubPrepOutputPlan&
        ) = default;
};

struct SubPrepGenerationPlan final
{
    std::vector<Domain::GregorianDate> normalizedDates;
    std::vector<Domain::ClassId> selectedClassIds;
    // These are the explicit weekday checks used for the information sheet.
    // The roster query has a separate date-derived weekday scope below.
    std::vector<SubPrepWeekday> selectedScheduleWeekdays;
    std::vector<SubPrepWeekday> rosterWeekdays;
    ScheduleViewMode mode = ScheduleViewMode::Regular;
    std::vector<std::string> requestedRosterColumns;
    SubPrepOutputPlan output;

    friend bool operator==(
        const SubPrepGenerationPlan&,
        const SubPrepGenerationPlan&
        ) = default;
};

struct SubPrepGenerationPlanResult final
{
    SubPrepGenerationPlanFailureReason reason =
        SubPrepGenerationPlanFailureReason::None;
    std::optional<SubPrepGenerationPlan> plan;

    [[nodiscard]] bool isValid() const noexcept
    {
        return reason == SubPrepGenerationPlanFailureReason::None
            && plan.has_value();
    }
};

[[nodiscard]] inline SubPrepGenerationPlanResult planSubPrepGeneration(
    const SubPrepGenerationPlanRequest& request
    )
{
    using Reason = SubPrepGenerationPlanFailureReason;

    const auto validation = validateSubPrepPackageRequest(
        {
            .createFolder = request.createFolder,
            .printPaperCopies = request.printPaperCopies,
            .hasValidSelectedDate = !request.selectedDates.empty(),
            .hasSelectedClassIds = !request.selectedClassIds.empty(),
            .hasNonEmptyTargetRoot = request.targetRootReady,
            .hasNonEmptyUserName = request.hasNonEmptyFolderLabel
        }
        );
    if (!validation.isValid())
    {
        switch (validation.reason)
        {
        case SubPrepPackageRequestValidationReason::NoOutputAction:
            return {Reason::NoOutputAction, std::nullopt};
        case SubPrepPackageRequestValidationReason::NoValidSelectedDate:
            return {Reason::NoValidSelectedDate, std::nullopt};
        case SubPrepPackageRequestValidationReason::NoSelectedClassIds:
            return {Reason::NoSelectedClassIds, std::nullopt};
        case SubPrepPackageRequestValidationReason::MissingTargetRoot:
            return {Reason::MissingTargetRoot, std::nullopt};
        case SubPrepPackageRequestValidationReason::MissingUserName:
            return {Reason::MissingUserName, std::nullopt};
        case SubPrepPackageRequestValidationReason::None:
            break;
        }
        return {Reason::None, std::nullopt};
    }

    for (const SubPrepWeekday weekday : request.selectedScheduleWeekdays)
    {
        if (!isValidSubPrepWeekday(weekday))
        {
            return {Reason::InvalidSelectedScheduleWeekday, std::nullopt};
        }
    }
    if (!isValidScheduleViewMode(request.mode))
    {
        return {Reason::InvalidScheduleMode, std::nullopt};
    }

    SubPrepGenerationPlan plan;
    plan.normalizedDates = request.selectedDates;
    std::sort(
        plan.normalizedDates.begin(),
        plan.normalizedDates.end(),
        [](const Domain::GregorianDate& left,
           const Domain::GregorianDate& right)
        {
            return left.dayNumber() < right.dayNumber();
        }
        );
    plan.normalizedDates.erase(
        std::unique(plan.normalizedDates.begin(), plan.normalizedDates.end()),
        plan.normalizedDates.end()
        );

    plan.selectedClassIds = request.selectedClassIds;
    for (const SubPrepWeekday weekday : request.selectedScheduleWeekdays)
    {
        if (std::find(
                plan.selectedScheduleWeekdays.cbegin(),
                plan.selectedScheduleWeekdays.cend(),
                weekday
                ) == plan.selectedScheduleWeekdays.cend())
        {
            plan.selectedScheduleWeekdays.push_back(weekday);
        }
    }

    for (const Domain::GregorianDate& date : plan.normalizedDates)
    {
        SubPrepWeekday weekday = SubPrepWeekday::Monday;
        switch (date.weekday())
        {
        case Domain::IsoWeekday::Monday:
            weekday = SubPrepWeekday::Monday;
            break;
        case Domain::IsoWeekday::Tuesday:
            weekday = SubPrepWeekday::Tuesday;
            break;
        case Domain::IsoWeekday::Wednesday:
            weekday = SubPrepWeekday::Wednesday;
            break;
        case Domain::IsoWeekday::Thursday:
            weekday = SubPrepWeekday::Thursday;
            break;
        case Domain::IsoWeekday::Friday:
            weekday = SubPrepWeekday::Friday;
            break;
        case Domain::IsoWeekday::Saturday:
            weekday = SubPrepWeekday::Saturday;
            break;
        case Domain::IsoWeekday::Sunday:
            weekday = SubPrepWeekday::Sunday;
            break;
        }

        if (std::find(
                plan.rosterWeekdays.cbegin(),
                plan.rosterWeekdays.cend(),
                weekday
                ) == plan.rosterWeekdays.cend())
        {
            plan.rosterWeekdays.push_back(weekday);
        }
    }

    plan.mode = request.mode;
    plan.requestedRosterColumns = request.requestedRosterColumns;
    plan.output.createFolder = request.createFolder;
    plan.output.printPaperCopies = request.printPaperCopies;
    if (request.createFolder)
    {
        plan.output.folder = SubPrepFolderOutputPlan{
            .folderLabel = request.folderLabel,
            .targetRootReady = request.targetRootReady,
            .replaceExisting = request.replaceExisting,
            .openAfterGeneration = request.openFolderAfterGeneration
        };
    }

    return {Reason::None, std::move(plan)};
}

} // namespace ClassMngr::Next::Application
